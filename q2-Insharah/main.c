//updated lol2
#include "attack_detection.h"

int main(int argc, char *argv[])
{
    int rank, size;

    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    if (rank == 0) {
        printf("\n+============================================================+\n");
        printf("|  MPI DISTRIBUTED ATTACK DETECTION - Q2                    |\n");
        printf("|  Dataset : UNSW-NB15 Network Traffic                      |\n");
        printf("|  Processes: %-3d                                           |\n", size);
        printf("+============================================================+\n");
    }

    /* ----------------------------------------------------------
     * SCATTER: distribute work counts
     * ---------------------------------------------------------- */
    int my_max_count = 0;
    scatter_work_counts(&my_max_count, rank, size);

    MPI_Barrier(MPI_COMM_WORLD);

    /* ----------------------------------------------------------
     * LOAD: each process loads its own slice of real dataset
     * ---------------------------------------------------------- */
    SuspiciousIP local_ips[MAX_IPS];
    memset(local_ips, 0, sizeof(local_ips));
    int local_ip_count = 0;

    load_real_dataset(local_ips, &local_ip_count, rank, size);

    if (local_ip_count > my_max_count) {
        local_ip_count = my_max_count;
    }

    printf("Process %d: loaded %d IPs from UNSW-NB15\n", rank, local_ip_count);

    MPI_Barrier(MPI_COMM_WORLD);

    /* ----------------------------------------------------------
     * REDUCE: aggregate summary statistics
     * ---------------------------------------------------------- */
    reduce_statistics(local_ips, local_ip_count);

    MPI_Barrier(MPI_COMM_WORLD);

    /* ----------------------------------------------------------
     * REQUIREMENT 1: Distributed attack detection (MPI_Allreduce)
     * ---------------------------------------------------------- */
    detect_distributed_attack_allreduce(local_ips, local_ip_count);

    MPI_Barrier(MPI_COMM_WORLD);

    /* ----------------------------------------------------------
     * REQUIREMENT 2: Error checking and validation
     * Build checksum input from actual loaded IP strings
     * ---------------------------------------------------------- */
    char log_data[512] = {0};
    int log_len = 0;

    for (int i = 0; i < local_ip_count; i++) {
        int remaining = (int)sizeof(log_data) - log_len;
        if (remaining <= MAX_IP_LENGTH) {
            break;
        }

        int written = snprintf(log_data + log_len,
                               (size_t)remaining,
                               "%s",
                               local_ips[i].ip_address);

        if (written < 0 || written >= remaining) {
            break;
        }

        log_len += written;
    }

    if (log_len == 0) {
        strcpy(log_data, "X");
        log_len = 1;
    }

    validate_log_processing(log_data, log_len);

    MPI_Barrier(MPI_COMM_WORLD);

    /* ----------------------------------------------------------
     * REQUIREMENT 3: Cross-check suspicious IPs
     * ---------------------------------------------------------- */
    cross_check_suspicious_ips(local_ips, local_ip_count);

    MPI_Barrier(MPI_COMM_WORLD);

    /* ----------------------------------------------------------
     * Save results on rank 0
     * ---------------------------------------------------------- */
    if (rank == 0) {
        printf("\n+============================================================+\n");
        printf("|  ANALYSIS COMPLETE                                         |\n");
        printf("+============================================================+\n\n");

        FILE *results = fopen("/tmp/q2_results.json", "w");
        if (results) {
            fprintf(results, "{\n");
            fprintf(results, "  \"status\": \"completed\",\n");
            fprintf(results, "  \"timestamp\": %ld,\n", (long)time(NULL));
            fprintf(results, "  \"processes\": %d,\n", size);
            fprintf(results, "  \"dataset\": \"UNSW-NB15\",\n");
            fprintf(results, "  \"mpi_operations\": [\n");
            fprintf(results, "    \"MPI_Scatter\",\n");
            fprintf(results, "    \"MPI_Reduce\",\n");
            fprintf(results, "    \"MPI_Allreduce\",\n");
            fprintf(results, "    \"MPI_Gather\",\n");
            fprintf(results, "    \"MPI_Gatherv\",\n");
            fprintf(results, "    \"MPI_Bcast\"\n");
            fprintf(results, "  ]\n");
            fprintf(results, "}\n");
            fclose(results);
            printf("Results saved to /tmp/q2_results.json\n");
        } else {
            fprintf(stderr, "Rank 0: failed to write /tmp/q2_results.json\n");
        }
    }

    MPI_Finalize();
    return 0;
}
