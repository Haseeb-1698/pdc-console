#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <mpi.h>

#define MAX_LINE 4096
#define MAX_IPS  1000

typedef struct {
    int backdoor_count;
    int dos_count;
    int recon_count;
    int suspicious_ip_count;
    char suspicious_ips[MAX_IPS][16];
} LocalResult;

void parse_csv_line(const char *line, char *attack_cat, char *srcip) {
    char *copy = malloc(strlen(line) + 1);
    strcpy(copy, line);
    char *saveptr;
    int col = 1;
    attack_cat[0] = '\0';
    srcip[0] = '\0';
    char *token = strtok_r(copy, ",", &saveptr);
    while (token != NULL && col <= 44) {
        if (col == 1)  strcpy(srcip, token);
        if (col == 44) strcpy(attack_cat, token);
        token = strtok_r(NULL, ",", &saveptr);
        col++;
    }
    free(copy);
}

int ip_exists(char ips[][16], int count, const char *ip) {
    for (int i = 0; i < count; i++)
        if (strcmp(ips[i], ip) == 0) return 1;
    return 0;
}

int main(int argc, char *argv[]) {
    MPI_Init(&argc, &argv);
    int rank, size;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    const char *filename = (argc > 1) ? argv[1] : "dataset/UNSW_NB15_testing-set.csv";
    LocalResult local_result = {0, 0, 0, 0};
    LocalResult global_result = {0, 0, 0, 0};

    int total_lines = 0;
    char **all_lines = NULL;

    if (rank == 0) {
        FILE *fp = fopen(filename, "r");
        if (!fp) {
            printf("ERROR: Cannot open file %s\n", filename);
            MPI_Abort(MPI_COMM_WORLD, 1);
        }
        char line[MAX_LINE];
        int capacity = 1024;
        all_lines = malloc(capacity * sizeof(char *));
        if (!fgets(line, sizeof(line), fp)) { MPI_Abort(MPI_COMM_WORLD, 1); }
        while (fgets(line, sizeof(line), fp)) {
            if (total_lines >= capacity) {
                capacity *= 2;
                all_lines = realloc(all_lines, capacity * sizeof(char *));
            }
            all_lines[total_lines] = malloc(strlen(line) + 1);
            strcpy(all_lines[total_lines], line);
            total_lines++;
        }
        fclose(fp);
        printf("PDC PROJECT Q1 - PARALLEL MALICIOUS ACTIVITY DETECTION\n");
        printf("Dataset : %s\n", filename);
        printf("Records : %d\n", total_lines);
        printf("Processes: %d\n\n", size);
    }

    /* MPI_BCAST: broadcast total line count to all processes */
    MPI_Bcast(&total_lines, 1, MPI_INT, 0, MPI_COMM_WORLD);

    int lines_per_proc = total_lines / size;
    int remainder      = total_lines % size;
    int my_count       = lines_per_proc + (rank < remainder ? 1 : 0);

    /* MPI_SCATTERV: distribute file lines to all processes */
    char *scatter_buffer = NULL;
    if (rank == 0) {
        scatter_buffer = malloc((size_t)total_lines * MAX_LINE);
        for (int i = 0; i < total_lines; i++)
            strncpy(scatter_buffer + i * MAX_LINE, all_lines[i], MAX_LINE - 1);
    }

    char *my_buffer = malloc((size_t)my_count * MAX_LINE);

    int *sendcounts = NULL, *displs = NULL;
    if (rank == 0) {
        sendcounts = malloc(size * sizeof(int));
        displs     = malloc(size * sizeof(int));
        int offset = 0;
        for (int p = 0; p < size; p++) {
            int p_count   = lines_per_proc + (p < remainder ? 1 : 0);
            sendcounts[p] = p_count * MAX_LINE;
            displs[p]     = offset;
            offset       += p_count * MAX_LINE;
        }
    }

    MPI_Scatterv(scatter_buffer, sendcounts, displs, MPI_CHAR,
                 my_buffer, my_count * MAX_LINE, MPI_CHAR, 0, MPI_COMM_WORLD);

    /* Each rank analyzes its chunk */
    for (int i = 0; i < my_count; i++) {
        char attack_cat[256] = "";
        char srcip[64] = "";
        parse_csv_line(my_buffer + i * MAX_LINE, attack_cat, srcip);

        int is_malicious = 0;
        if (strcmp(attack_cat, "Backdoor") == 0) {
            local_result.backdoor_count++;
            is_malicious = 1;
        } else if (strcmp(attack_cat, "DoS") == 0) {
            local_result.dos_count++;
            is_malicious = 1;
        } else if (strcmp(attack_cat, "Reconnaissance") == 0) {
            local_result.recon_count++;
            is_malicious = 1;
        }
        if (is_malicious && local_result.suspicious_ip_count < MAX_IPS) {
            if (!ip_exists(local_result.suspicious_ips, local_result.suspicious_ip_count, srcip)) {
                snprintf(local_result.suspicious_ips[local_result.suspicious_ip_count], 16, "%s", srcip);
                local_result.suspicious_ip_count++;
            }
        }
    }

    /* MPI_REDUCE: sum attack counts to master */
    MPI_Reduce(&local_result.backdoor_count, &global_result.backdoor_count, 1, MPI_INT, MPI_SUM, 0, MPI_COMM_WORLD);
    MPI_Reduce(&local_result.dos_count,      &global_result.dos_count,      1, MPI_INT, MPI_SUM, 0, MPI_COMM_WORLD);
    MPI_Reduce(&local_result.recon_count,    &global_result.recon_count,    1, MPI_INT, MPI_SUM, 0, MPI_COMM_WORLD);

    /* MPI_ALLREDUCE: aggregate suspicious IP count visible to all ranks */
    int global_suspicious = 0;
    MPI_Allreduce(&local_result.suspicious_ip_count, &global_suspicious, 1, MPI_INT, MPI_SUM, MPI_COMM_WORLD);

    /* MPI_GATHER: collect suspicious IP lists from all processes to master */
    char *gathered_ips = NULL;
    if (rank == 0)
        gathered_ips = malloc((size_t)size * MAX_IPS * 16);

    MPI_Gather(local_result.suspicious_ips, MAX_IPS * 16, MPI_CHAR,
               gathered_ips,               MAX_IPS * 16, MPI_CHAR,
               0, MPI_COMM_WORLD);

    /* Per-rank breakdown */
    if (rank == 0) printf("=== RANK-WISE BREAKDOWN ===\n");
    for (int p = 0; p < size; p++) {
        MPI_Barrier(MPI_COMM_WORLD);
        if (rank == p)
            printf("Rank %d: lines=%d  Backdoor=%d  DoS=%d  Recon=%d  SuspiciousIPs=%d\n",
                   rank, my_count, local_result.backdoor_count,
                   local_result.dos_count, local_result.recon_count,
                   local_result.suspicious_ip_count);
    }

    /* Rank 0: deduplicate gathered IPs, then broadcast final list */
    char unique_ips[MAX_IPS * 4][16];
    int  unique_count = 0;

    if (rank == 0) {
        for (int p = 0; p < size; p++) {
            for (int i = 0; i < MAX_IPS && unique_count < MAX_IPS * 4; i++) {
                char *ip = gathered_ips + (size_t)p * MAX_IPS * 16 + i * 16;
                if (ip[0] == '\0') continue;
                if (!ip_exists(unique_ips, unique_count, ip)) {
                    strncpy(unique_ips[unique_count], ip, 15);
                    unique_ips[unique_count][15] = '\0';
                    unique_count++;
                }
            }
        }
    }

    /* MPI_BCAST: broadcast final attack results + unique IP count to all processes */
    int final_results[4];
    if (rank == 0) {
        final_results[0] = global_result.backdoor_count;
        final_results[1] = global_result.dos_count;
        final_results[2] = global_result.recon_count;
        final_results[3] = unique_count;
    }
    MPI_Bcast(final_results, 4, MPI_INT, 0, MPI_COMM_WORLD);

    if (rank == 0) {
        printf("\n=== GLOBAL RESULTS ===\n");
        printf("Total Backdoor attacks  : %d\n", final_results[0]);
        printf("Total DoS attacks       : %d\n", final_results[1]);
        printf("Total Reconnaissance    : %d\n", final_results[2]);
        printf("Total malicious records : %d\n",
               final_results[0] + final_results[1] + final_results[2]);
        printf("Unique suspicious IPs   : %d (deduplicated across all ranks)\n",
               final_results[3]);

        printf("\n=== FLAGGED IPs (sample, up to 20) ===\n");
        int show = unique_count < 20 ? unique_count : 20;
        for (int i = 0; i < show; i++)
            printf("  %s\n", unique_ips[i]);
        if (unique_count > 20)
            printf("  ... and %d more\n", unique_count - 20);
    }

    /* Cleanup */
    if (rank == 0) {
        if (scatter_buffer) free(scatter_buffer);
        if (sendcounts)     free(sendcounts);
        if (displs)         free(displs);
        if (gathered_ips)   free(gathered_ips);
        if (all_lines) {
            for (int i = 0; i < total_lines; i++) free(all_lines[i]);
            free(all_lines);
        }
    }
    free(my_buffer);

    MPI_Finalize();
    return 0;
}
