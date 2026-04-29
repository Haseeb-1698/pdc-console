//updated lol2
#include "attack_detection.h"

/* ================================================================
 * HELPER UTILITIES
 * ================================================================ */

static MPI_Datatype create_suspicious_ip_type(void)
{
    MPI_Datatype ip_type;

    int blockcounts[5] = {MAX_IP_LENGTH, 1, 1, 1, 1};
    MPI_Aint offsets[5];
    MPI_Datatype types[5] = {MPI_CHAR, MPI_INT, MPI_INT, MPI_INT, MPI_INT};

    offsets[0] = offsetof(SuspiciousIP, ip_address);
    offsets[1] = offsetof(SuspiciousIP, failed_logins);
    offsets[2] = offsetof(SuspiciousIP, port_scans);
    offsets[3] = offsetof(SuspiciousIP, connection_attempts);
    offsets[4] = offsetof(SuspiciousIP, is_suspicious);

    MPI_Type_create_struct(5, blockcounts, offsets, types, &ip_type);
    MPI_Type_commit(&ip_type);

    return ip_type;
}

void parse_log_line(const char *line, LogEntry *entry)
{
    memset(entry, 0, sizeof(LogEntry));

    char dstip[MAX_IP_LENGTH] = {0};
    int dsport = 0;
    char proto[20] = {0};
    char state[10] = {0};

    /* Basic CSV-style parse from UNSW-like rows */
    int parsed = sscanf(line,
                        "%15[^,],%d,%15[^,],%d,%19[^,],%9[^,]",
                        entry->source_ip,
                        &entry->port,
                        dstip,
                        &dsport,
                        proto,
                        state);

    if (parsed < 4) {
        strncpy(entry->event_type, "unknown", sizeof(entry->event_type) - 1);
        entry->timestamp = (int)time(NULL);
        strncpy(entry->log_line, line, sizeof(entry->log_line) - 1);
        return;
    }

    entry->timestamp = (int)time(NULL);
    strncpy(entry->log_line, line, sizeof(entry->log_line) - 1);

    if (strcmp(state, "CON") == 0 && entry->port > 10000) {
        strncpy(entry->event_type, "login", sizeof(entry->event_type) - 1);
    } else if (strcmp(state, "RST") == 0 || strcmp(state, "INT") == 0) {
        strncpy(entry->event_type, "connection", sizeof(entry->event_type) - 1);
    } else if (strstr(proto, "tcp") != NULL && entry->port > 1024) {
        strncpy(entry->event_type, "port_scan", sizeof(entry->event_type) - 1);
    } else {
        strncpy(entry->event_type, "unknown", sizeof(entry->event_type) - 1);
    }
}

int is_suspicious_ip(SuspiciousIP *ip_list, int list_size, const char *ip)
{
    for (int i = 0; i < list_size; i++) {
        if (strcmp(ip_list[i].ip_address, ip) == 0) {
            return i;
        }
    }
    return -1;
}

void add_suspicious_ip(SuspiciousIP *ip_list, int *list_size, const char *ip)
{
    if (*list_size >= MAX_IPS) {
        fprintf(stderr, "Warning: suspicious IP list full (MAX_IPS=%d)\n", MAX_IPS);
        return;
    }

    SuspiciousIP *entry = &ip_list[*list_size];
    memset(entry, 0, sizeof(SuspiciousIP));
    strncpy(entry->ip_address, ip, MAX_IP_LENGTH - 1);
    (*list_size)++;
}

void update_ip_stats(SuspiciousIP *ip_list, int list_size, const char *ip, const char *event_type)
{
    int idx = is_suspicious_ip(ip_list, list_size, ip);
    if (idx == -1) {
        return;
    }

    if (strcmp(event_type, "login") == 0) {
        ip_list[idx].failed_logins++;
    } else if (strcmp(event_type, "port_scan") == 0) {
        ip_list[idx].port_scans++;
    } else if (strcmp(event_type, "connection") == 0) {
        ip_list[idx].connection_attempts++;
    }

    if (ip_list[idx].failed_logins > 2 ||
        ip_list[idx].port_scans > 2 ||
        ip_list[idx].connection_attempts > 2) {
        ip_list[idx].is_suspicious = 1;
    }
}

unsigned int calculate_checksum(const char *data, int length)
{
    unsigned int crc = 0xFFFFFFFF;

    for (int i = 0; i < length; i++) {
        unsigned char byte = (unsigned char)data[i];
        crc ^= byte;

        for (int j = 0; j < 8; j++) {
            if (crc & 1U) {
                crc = (crc >> 1) ^ CHECKSUM_POLY;
            } else {
                crc >>= 1;
            }
        }
    }

    return crc ^ 0xFFFFFFFF;
}

void print_suspicious_ips(SuspiciousIP *ips, int count)
{
    printf("\n=== SUSPICIOUS IPs ===\n");
    printf("%-16s | Failed Logins | Port Scans | Connections | Status\n", "IP Address");
    printf("-------------------------------------------------------------------\n");

    int printed = 0;
    for (int i = 0; i < count; i++) {
        if (ips[i].is_suspicious) {
            printf("%-16s | %13d | %10d | %11d | FLAGGED\n",
                   ips[i].ip_address,
                   ips[i].failed_logins,
                   ips[i].port_scans,
                   ips[i].connection_attempts);
            printed++;
        }
    }

    if (printed == 0) {
        printf("  (none)\n");
    }
}

/* ================================================================
 * DATASET LOADING
 * Partitioning strategy for any process count:
 * 1) Assign each rank to one shard file by (rank % num_files).
 * 2) Ranks mapped to the same shard form a local group.
 * 3) Inside that shard, each rank reads lines where
 *    (line_number % group_size == group_rank).
 * This guarantees disjoint line ownership within a shard and
 * complete shard coverage without dropping lines.
 * ================================================================ */
void load_real_dataset(SuspiciousIP *ips, int *ip_count, int process_rank, int process_size)
{
    *ip_count = 0;

    const char *data_files[] = {
        "dataset/UNSW-NB15_1.csv",
        "dataset/UNSW-NB15_2.csv",
        "dataset/UNSW-NB15_3.csv",
        "dataset/UNSW-NB15_4.csv"
    };

    int num_files = (int)(sizeof(data_files) / sizeof(data_files[0]));
    int file_index = process_rank % num_files;
    const char *selected_file = data_files[file_index];

    int group_size = 0;
    int group_rank = 0;
    for (int r = file_index; r < process_size; r += num_files) {
        if (r == process_rank) {
            group_rank = group_size;
        }
        group_size++;
    }
    if (group_size <= 0) {
        return;
    }

    FILE *fp = fopen(selected_file, "r");
    if (!fp) {
        fprintf(stderr, "Process %d: cannot open dataset file %s\n", process_rank, selected_file);
        return;
    }

    char line[1024];
    int line_count = 0;

    /* Skip header */
    if (fgets(line, sizeof(line), fp) == NULL) {
        fclose(fp);
        return;
    }

    while (fgets(line, sizeof(line), fp) != NULL && *ip_count < MAX_IPS) {
        line_count++;

        if ((line_count % group_size) != group_rank) {
            continue;
        }

        char srcip[MAX_IP_LENGTH] = {0};
        int sport = 0;
        char proto[20] = {0};
        char state[10] = {0};
        int sbytes = 0;

        /*
         * Approximate UNSW-NB15 parsing:
         * srcip,sport,dstip,dsport,proto,state,dur,sbytes,...
         */
        int parsed = sscanf(line,
                            "%15[^,],%d,%*[^,],%*d,%19[^,],%9[^,],%*f,%d",
                            srcip, &sport, proto, state, &sbytes);

        if (parsed < 4 || srcip[0] == '\0') {
            continue;
        }

        int idx = is_suspicious_ip(ips, *ip_count, srcip);
        if (idx == -1) {
            add_suspicious_ip(ips, ip_count, srcip);
            idx = *ip_count - 1;
            if (idx < 0 || idx >= *ip_count) {
                continue;
            }
        }

        /* Heuristic detection logic */
        if (strcmp(state, "RST") == 0 || strcmp(state, "INT") == 0) {
            ips[idx].connection_attempts++;
        }

        if (strstr(proto, "tcp") != NULL && sport > 1024 && sbytes > 10000) {
            ips[idx].port_scans++;
        }

        if (strcmp(state, "CON") == 0 && sport > 10000) {
            ips[idx].failed_logins++;
        }

        if (ips[idx].failed_logins > 2 ||
            ips[idx].port_scans > 2 ||
            ips[idx].connection_attempts > 2) {
            ips[idx].is_suspicious = 1;
        }
    }

    fclose(fp);
}

/* ================================================================
 * MPI_SCATTER
 * Distribute per-process work counts from rank 0
 * ================================================================ */
void scatter_work_counts(int *my_count, int rank, int size)
{
    int *counts = NULL;

    if (rank == 0) {
        counts = (int *)malloc(size * sizeof(int));
        if (!counts) {
            fprintf(stderr, "Rank 0: failed to allocate scatter counts\n");
            MPI_Abort(MPI_COMM_WORLD, 1);
        }

        int base = MAX_IPS / size;
        int extra = MAX_IPS % size;

        printf("\n========================================\n");
        printf("SCATTER: Distributing work counts (MPI_Scatter)\n");
        printf("========================================\n");

        for (int i = 0; i < size; i++) {
            counts[i] = base + (i < extra ? 1 : 0);
            printf("  Process %d -> up to %d IPs\n", i, counts[i]);
        }
    }

    MPI_Scatter(counts, 1, MPI_INT, my_count, 1, MPI_INT, 0, MPI_COMM_WORLD);

    if (rank == 0) {
        printf("Completed MPI_Scatter\n");
        free(counts);
    }
}

/* ================================================================
 * MPI_REDUCE
 * Aggregate summary statistics onto rank 0
 * ================================================================ */
int reduce_statistics(SuspiciousIP *local_ips, int local_count)
{
    int rank;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);

    int local_suspicious = 0;
    int local_failed = 0;
    int local_scans = 0;
    int local_connections = 0;

    for (int i = 0; i < local_count; i++) {
        if (local_ips[i].is_suspicious) {
            local_suspicious++;
        }
        local_failed += local_ips[i].failed_logins;
        local_scans += local_ips[i].port_scans;
        local_connections += local_ips[i].connection_attempts;
    }

    int global_max_suspicious = 0;
    int global_failed = 0;
    int global_scans = 0;
    int global_connections = 0;

    MPI_Reduce(&local_suspicious, &global_max_suspicious, 1, MPI_INT, MPI_MAX, 0, MPI_COMM_WORLD);
    MPI_Reduce(&local_failed, &global_failed, 1, MPI_INT, MPI_SUM, 0, MPI_COMM_WORLD);
    MPI_Reduce(&local_scans, &global_scans, 1, MPI_INT, MPI_SUM, 0, MPI_COMM_WORLD);
    MPI_Reduce(&local_connections, &global_connections, 1, MPI_INT, MPI_SUM, 0, MPI_COMM_WORLD);

    if (rank == 0) {
        printf("\n========================================\n");
        printf("REDUCE: Aggregating statistics (MPI_Reduce)\n");
        printf("========================================\n");
        printf("Max suspicious IPs in one process (MAX) : %d\n", global_max_suspicious);
        printf("Total failed logins               (SUM) : %d\n", global_failed);
        printf("Total port scans                  (SUM) : %d\n", global_scans);
        printf("Total connection attempts         (SUM) : %d\n", global_connections);
        printf("Completed MPI_Reduce\n");

        return global_failed + global_scans + global_connections;
    }

    return 0;
}

/* ================================================================
 * REQUIREMENT 1
 * MPI_Allreduce for global totals visible to all ranks
 * ================================================================ */
int detect_distributed_attack_allreduce(SuspiciousIP *local_ips, int local_count)
{
    int rank;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);

    int local_suspicious = 0;
    int local_failed = 0;
    int local_scans = 0;
    int local_connections = 0;

    for (int i = 0; i < local_count; i++) {
        if (local_ips[i].is_suspicious) {
            local_suspicious++;
        }
        local_failed += local_ips[i].failed_logins;
        local_scans += local_ips[i].port_scans;
        local_connections += local_ips[i].connection_attempts;
    }

    int global_suspicious = 0;
    int global_failed = 0;
    int global_scans = 0;
    int global_connections = 0;

    MPI_Allreduce(&local_suspicious, &global_suspicious, 1, MPI_INT, MPI_SUM, MPI_COMM_WORLD);
    MPI_Allreduce(&local_failed, &global_failed, 1, MPI_INT, MPI_SUM, MPI_COMM_WORLD);
    MPI_Allreduce(&local_scans, &global_scans, 1, MPI_INT, MPI_SUM, MPI_COMM_WORLD);
    MPI_Allreduce(&local_connections, &global_connections, 1, MPI_INT, MPI_SUM, MPI_COMM_WORLD);

    int attack_detected = (global_failed > 10 || global_scans > 20 || global_connections > 30);

    if (rank == 0) {
        printf("\n========================================\n");
        printf("REQUIREMENT 1: DISTRIBUTED ATTACK DETECTION (MPI_Allreduce)\n");
        printf("========================================\n");
        printf("Global suspicious IPs   : %d\n", global_suspicious);
        printf("Total failed logins     : %d\n", global_failed);
        printf("Total port scans        : %d\n", global_scans);
        printf("Total connections       : %d\n", global_connections);

        if (attack_detected) {
            printf("\nPOTENTIAL DDoS / PORT-SCANNING ATTACK DETECTED!\n");
        } else {
            printf("\nAttack indicators within normal range\n");
        }
    }

    return attack_detected;
}

/* ================================================================
 * REQUIREMENT 2
 * MPI_Gather + MPI_Reduce for validation/checksum
 * ================================================================ */
int validate_log_processing(const char *local_data, int local_length)
{
    int rank, size;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    unsigned int local_checksum = calculate_checksum(local_data, local_length);

    unsigned int *all_checksums = NULL;
    int *all_lengths = NULL;

    if (rank == 0) {
        all_checksums = (unsigned int *)malloc(size * sizeof(unsigned int));
        all_lengths = (int *)malloc(size * sizeof(int));
        if (!all_checksums || !all_lengths) {
            fprintf(stderr, "Rank 0: failed to allocate validation buffers\n");
            MPI_Abort(MPI_COMM_WORLD, 1);
        }
    }

    MPI_Gather(&local_checksum, 1, MPI_UNSIGNED,
               all_checksums, 1, MPI_UNSIGNED,
               0, MPI_COMM_WORLD);

    MPI_Gather(&local_length, 1, MPI_INT,
               all_lengths, 1, MPI_INT,
               0, MPI_COMM_WORLD);

    unsigned int global_xor = 0;
    MPI_Reduce(&local_checksum, &global_xor, 1, MPI_UNSIGNED, MPI_BXOR, 0, MPI_COMM_WORLD);

    int validation_passed = 1;

    if (rank == 0) {
        printf("\n========================================\n");
        printf("REQUIREMENT 2: ERROR CHECKING & VALIDATION (MPI_Gather + MPI_Reduce)\n");
        printf("========================================\n");
        printf("Global XOR checksum (MPI_Reduce): 0x%08X\n\n", global_xor);
        printf("Per-process checksum report:\n");

        for (int i = 0; i < size; i++) {
            printf("  Process %d : 0x%08X (%d bytes)\n",
                   i, all_checksums[i], all_lengths[i]);
        }

        printf("\nValidation: ");
        for (int i = 0; i < size - 1; i++) {
            for (int j = i + 1; j < size; j++) {
                if (all_checksums[i] == all_checksums[j] &&
                    all_lengths[i] == all_lengths[j]) {
                    printf("\nWARNING: processes %d and %d have identical checksum/length; possible duplicate segment!\n", i, j);
                    validation_passed = 0;
                }
            }
        }

        if (validation_passed) {
            printf("PASSED - all processes handled distinct log segments\n");
        } else {
            printf("FAILED - possible duplication detected\n");
        }

        free(all_checksums);
        free(all_lengths);
    }

    MPI_Bcast(&validation_passed, 1, MPI_INT, 0, MPI_COMM_WORLD);
    return validation_passed;
}

/* ================================================================
 * REQUIREMENT 3
 * MPI_Gatherv + MPI_Bcast for suspicious IP cross-checking
 * ================================================================ */
void cross_check_suspicious_ips(SuspiciousIP *local_ips, int local_count)
{
    int rank, size;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    MPI_Datatype ip_type = create_suspicious_ip_type();

    SuspiciousIP *local_suspicious = (SuspiciousIP *)malloc(MAX_IPS * sizeof(SuspiciousIP));
    if (!local_suspicious) {
        fprintf(stderr, "Process %d: failed to allocate local suspicious buffer\n", rank);
        MPI_Abort(MPI_COMM_WORLD, 1);
    }

    int local_suspicious_count = 0;
    for (int i = 0; i < local_count; i++) {
        if (local_ips[i].is_suspicious) {
            local_suspicious[local_suspicious_count++] = local_ips[i];
        }
    }

    int *suspicious_counts = NULL;
    int *displacements = NULL;

    if (rank == 0) {
        suspicious_counts = (int *)malloc(size * sizeof(int));
        displacements = (int *)malloc(size * sizeof(int));
        if (!suspicious_counts || !displacements) {
            fprintf(stderr, "Rank 0: failed to allocate gather metadata\n");
            MPI_Abort(MPI_COMM_WORLD, 1);
        }
    }

    MPI_Gather(&local_suspicious_count, 1, MPI_INT,
               suspicious_counts, 1, MPI_INT,
               0, MPI_COMM_WORLD);

    int total_suspicious = 0;

    if (rank == 0) {
        printf("\n========================================\n");
        printf("REQUIREMENT 3: CROSS-CHECKING SUSPICIOUS IPs (MPI_Gatherv + MPI_Bcast)\n");
        printf("========================================\n");
        printf("Suspicious IP counts per process:\n");

        displacements[0] = 0;
        for (int i = 0; i < size; i++) {
            printf("  Process %d : %d suspicious IPs\n", i, suspicious_counts[i]);
            if (i > 0) {
                displacements[i] = displacements[i - 1] + suspicious_counts[i - 1];
            }
            total_suspicious += suspicious_counts[i];
        }
    }

    SuspiciousIP *all_suspicious = NULL;
    if (rank == 0 && total_suspicious > 0) {
        all_suspicious = (SuspiciousIP *)malloc(total_suspicious * sizeof(SuspiciousIP));
        if (!all_suspicious) {
            fprintf(stderr, "Rank 0: failed to allocate all_suspicious buffer\n");
            MPI_Abort(MPI_COMM_WORLD, 1);
        }
    }

    MPI_Gatherv(local_suspicious, local_suspicious_count, ip_type,
                all_suspicious, suspicious_counts, displacements, ip_type,
                0, MPI_COMM_WORLD);

    SuspiciousIP final_list[MAX_IPS];
    memset(final_list, 0, sizeof(final_list));
    int final_count = 0;

    if (rank == 0) {
        printf("\nDeduplication and merge:\n");

        for (int i = 0; i < total_suspicious; i++) {
            int existing = is_suspicious_ip(final_list, final_count, all_suspicious[i].ip_address);

            if (existing == -1) {
                if (final_count < MAX_IPS) {
                    final_list[final_count++] = all_suspicious[i];
                } else {
                    fprintf(stderr, "Warning: final suspicious IP list reached MAX_IPS limit\n");
                    break;
                }
            } else {
                final_list[existing].failed_logins += all_suspicious[i].failed_logins;
                final_list[existing].port_scans += all_suspicious[i].port_scans;
                final_list[existing].connection_attempts += all_suspicious[i].connection_attempts;
                final_list[existing].is_suspicious = 1;
                printf("  Merged duplicate: %s\n", all_suspicious[i].ip_address);
            }
        }

        printf("\nUnique suspicious IPs after deduplication: %d\n", final_count);
        print_suspicious_ips(final_list, final_count);
        printf("\nBroadcasting final list to all %d processes...\n", size);
    }

    MPI_Bcast(&final_count, 1, MPI_INT, 0, MPI_COMM_WORLD);
    MPI_Bcast(final_list, MAX_IPS, ip_type, 0, MPI_COMM_WORLD);

    if (rank == 0) {
        printf("All processes now hold the authoritative suspicious-IP list\n");
    }

    if (all_suspicious) {
        free(all_suspicious);
    }
    if (suspicious_counts) {
        free(suspicious_counts);
    }
    if (displacements) {
        free(displacements);
    }
    free(local_suspicious);
    MPI_Type_free(&ip_type);
}
