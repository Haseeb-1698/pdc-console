/*
 * Q3: Performance Analysis - PDC Semester Project
 * Author: Haseeb
 *
 * Complete serial + parallel log analysis with MPI_Wtime benchmarking.
 * Detects Backdoor, DoS, Reconnaissance attacks in UNSW-NB15 dataset.
 *
 * Compile: mpicc -Wall -O2 q3-haseeb/main.c -o q3 -lm
 * Run:     mpirun --oversubscribe -np <N> ./q3 dataset/UNSW_NB15_training-set.csv
 *
 * Supports: UNSW_NB15_training-set.csv, UNSW_NB15_testing-set.csv
 *           (CSV files with header row, attack_cat as 2nd-to-last column)
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <mpi.h>

#define MAX_LINE_LEN  512
#define MAX_LINES     300000
#define NUM_ATTACKS   3

#define ATK_BACKDOOR  0
#define ATK_DOS       1
#define ATK_RECON     2

static const char *ATK_NAMES[NUM_ATTACKS] = {"Backdoor", "DoS", "Reconnaissance"};

/* ----------------------------------------------------------------
 * classify_attack - extract attack_cat from CSV line, return type
 * Returns 0=Backdoor, 1=DoS, 2=Recon, -1=other/normal
 * attack_cat is the second-to-last comma-separated field
 * ---------------------------------------------------------------- */
static int classify_attack(const char *line)
{
    int len = (int)strlen(line);
    int comma1 = -1, comma2 = -1, n = 0;

    for (int i = len - 1; i >= 0; i--) {
        if (line[i] == ',') {
            n++;
            if (n == 1) comma1 = i;
            else if (n == 2) { comma2 = i; break; }
        }
    }
    if (comma2 < 0 || comma1 <= comma2) return -1;

    const char *cat = line + comma2 + 1;
    int cat_len = comma1 - comma2 - 1;

    while (cat_len > 0 && *cat == ' ') { cat++; cat_len--; }
    while (cat_len > 0 && (cat[cat_len-1] == ' ' || cat[cat_len-1] == '\r'))
        cat_len--;
    if (cat_len <= 0) return -1;

    if (cat_len >= 8  && strncmp(cat, "Backdoor", 8) == 0)        return ATK_BACKDOOR;
    if (cat_len >= 9  && strncmp(cat, "Backdoors", 9) == 0)       return ATK_BACKDOOR;
    if (cat_len == 3  && strncmp(cat, "DoS", 3) == 0)             return ATK_DOS;
    if (cat_len >= 14 && strncmp(cat, "Reconnaissance", 14) == 0) return ATK_RECON;

    return -1;
}

/* djb2 hash for error-checking validation (checksum) */
static unsigned long line_checksum(const char *line)
{
    unsigned long hash = 5381;
    while (*line) {
        hash = ((hash << 5) + hash) + (unsigned char)(*line);
        line++;
    }
    return hash;
}

/* ----------------------------------------------------------------
 * serial_analysis - single-process baseline (no MPI collective calls)
 * ---------------------------------------------------------------- */
static void serial_analysis(char lines[][MAX_LINE_LEN], int total,
                            int counts[NUM_ATTACKS], unsigned long *checksum,
                            double *time_out)
{
    double t0 = MPI_Wtime();
    counts[0] = counts[1] = counts[2] = 0;
    *checksum = 0;

    for (int i = 0; i < total; i++) {
        int type = classify_attack(lines[i]);
        if (type >= 0) counts[type]++;
        *checksum += line_checksum(lines[i]);
    }

    *time_out = MPI_Wtime() - t0;
}

/* ================================================================
 * MAIN
 * ================================================================ */
int main(int argc, char *argv[])
{
    MPI_Init(&argc, &argv);

    int rank, size;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    if (argc < 2) {
        if (rank == 0)
            fprintf(stderr,
                "Usage: mpirun -np <N> ./q3 <csv_file>\n"
                "  e.g.: mpirun --oversubscribe -np 4 ./q3 dataset/UNSW_NB15_training-set.csv\n");
        MPI_Finalize();
        return 1;
    }

    /* ----------------------------------------------------------
     * TIMING VARIABLES
     * ---------------------------------------------------------- */
    double t_file_read = 0, t_serial    = 0;
    double t_scatter   = 0, t_compute   = 0;
    double t_reduce    = 0, t_allreduce = 0;
    double t_gather    = 0, t_bcast     = 0;
    double t_parallel  = 0, t_program;
    double t_program_start = MPI_Wtime();
    double t0;

    /* ----------------------------------------------------------
     * PHASE 1: FILE READING (rank 0)
     * ---------------------------------------------------------- */
    int total_lines = 0, padded_lines, chunk_size;
    char (*all_lines)[MAX_LINE_LEN] = NULL;

    t0 = MPI_Wtime();
    if (rank == 0) {
        FILE *fp = fopen(argv[1], "r");
        if (!fp) {
            fprintf(stderr, "Error: Cannot open '%s'\n", argv[1]);
            MPI_Abort(MPI_COMM_WORLD, 1);
        }

        all_lines = malloc((size_t)MAX_LINES * MAX_LINE_LEN);
        if (!all_lines) {
            fprintf(stderr, "Error: malloc failed\n");
            MPI_Abort(MPI_COMM_WORLD, 1);
        }
        memset(all_lines, 0, (size_t)MAX_LINES * MAX_LINE_LEN);

        char buf[MAX_LINE_LEN];
        /* Skip header row (and BOM if present) */
        if (fgets(buf, MAX_LINE_LEN, fp) == NULL) {
            fprintf(stderr, "Error: empty file\n");
            MPI_Abort(MPI_COMM_WORLD, 1);
        }

        while (fgets(buf, MAX_LINE_LEN, fp) && total_lines < MAX_LINES) {
            int l = (int)strlen(buf);
            while (l > 0 && (buf[l-1] == '\n' || buf[l-1] == '\r'))
                buf[--l] = '\0';
            if (l > 0) {
                memcpy(all_lines[total_lines], buf, l + 1);
                total_lines++;
            }
        }
        fclose(fp);
    }
    t_file_read = MPI_Wtime() - t0;

    /* Broadcast line count to all ranks */
    MPI_Bcast(&total_lines, 1, MPI_INT, 0, MPI_COMM_WORLD);

    /* Pad to multiple of size for even MPI_Scatter */
    padded_lines = total_lines;
    if (padded_lines % size != 0)
        padded_lines += size - (padded_lines % size);
    chunk_size = padded_lines / size;

    /* ----------------------------------------------------------
     * PHASE 2: SERIAL ANALYSIS (rank 0, baseline for comparison)
     * ---------------------------------------------------------- */
    int serial_counts[NUM_ATTACKS] = {0};
    unsigned long serial_checksum = 0;

    if (rank == 0) {
        serial_analysis(all_lines, total_lines,
                        serial_counts, &serial_checksum, &t_serial);

        /* Pad buffer for scatter */
        if (padded_lines > total_lines) {
            all_lines = realloc(all_lines, (size_t)padded_lines * MAX_LINE_LEN);
            for (int i = total_lines; i < padded_lines; i++)
                memset(all_lines[i], 0, MAX_LINE_LEN);
        }
    }

    /* ----------------------------------------------------------
     * PHASE 3: PARALLEL ANALYSIS (all ranks)
     * ---------------------------------------------------------- */
    double t_par_start = MPI_Wtime();

    /* --- MPI_Scatter: distribute log chunks to all processes --- */
    char (*local_lines)[MAX_LINE_LEN] = malloc((size_t)chunk_size * MAX_LINE_LEN);
    if (!local_lines) {
        fprintf(stderr, "Rank %d: malloc failed\n", rank);
        MPI_Abort(MPI_COMM_WORLD, 1);
    }

    t0 = MPI_Wtime();
    MPI_Scatter(all_lines, chunk_size * MAX_LINE_LEN, MPI_CHAR,
                local_lines, chunk_size * MAX_LINE_LEN, MPI_CHAR,
                0, MPI_COMM_WORLD);
    t_scatter = MPI_Wtime() - t0;

    /* --- Local computation: scan chunk for attacks --- */
    t0 = MPI_Wtime();

    int local_counts[NUM_ATTACKS] = {0};
    unsigned long local_checksum = 0;

    for (int i = 0; i < chunk_size; i++) {
        if (local_lines[i][0] == '\0') continue;   /* skip padding lines */
        int type = classify_attack(local_lines[i]);
        if (type >= 0) local_counts[type]++;
        local_checksum += line_checksum(local_lines[i]);
    }
    t_compute = MPI_Wtime() - t0;

    /* --- MPI_Reduce: sum attack counts to master (rank 0) --- */
    int global_counts[NUM_ATTACKS] = {0};
    t0 = MPI_Wtime();
    MPI_Reduce(local_counts, global_counts, NUM_ATTACKS, MPI_INT,
               MPI_SUM, 0, MPI_COMM_WORLD);
    t_reduce = MPI_Wtime() - t0;

    /* --- MPI_Allreduce: aggregate checksum + total attacks across ALL --- */
    unsigned long global_checksum = 0;
    int local_total  = local_counts[0] + local_counts[1] + local_counts[2];
    int global_total = 0;

    t0 = MPI_Wtime();
    MPI_Allreduce(&local_checksum, &global_checksum, 1,
                  MPI_UNSIGNED_LONG, MPI_SUM, MPI_COMM_WORLD);
    MPI_Allreduce(&local_total, &global_total, 1,
                  MPI_INT, MPI_SUM, MPI_COMM_WORLD);
    t_allreduce = MPI_Wtime() - t0;

    /* --- MPI_Gather: collect per-rank results to master --- */
    int *gathered = NULL;
    if (rank == 0)
        gathered = malloc(size * NUM_ATTACKS * sizeof(int));

    t0 = MPI_Wtime();
    MPI_Gather(local_counts, NUM_ATTACKS, MPI_INT,
               gathered,     NUM_ATTACKS, MPI_INT,
               0, MPI_COMM_WORLD);
    t_gather = MPI_Wtime() - t0;

    /* --- MPI_Bcast: broadcast final results to all processes --- */
    t0 = MPI_Wtime();
    MPI_Bcast(global_counts, NUM_ATTACKS, MPI_INT, 0, MPI_COMM_WORLD);
    t_bcast = MPI_Wtime() - t0;

    t_parallel = MPI_Wtime() - t_par_start;
    t_program  = MPI_Wtime() - t_program_start;

    /* ----------------------------------------------------------
     * PHASE 4: COLLECT TIMING FROM ALL RANKS (max across ranks)
     * ---------------------------------------------------------- */
    double max_scatter, max_compute, max_reduce;
    double max_allreduce, max_gather, max_bcast;

    MPI_Reduce(&t_scatter,   &max_scatter,   1, MPI_DOUBLE, MPI_MAX, 0, MPI_COMM_WORLD);
    MPI_Reduce(&t_compute,   &max_compute,   1, MPI_DOUBLE, MPI_MAX, 0, MPI_COMM_WORLD);
    MPI_Reduce(&t_reduce,    &max_reduce,    1, MPI_DOUBLE, MPI_MAX, 0, MPI_COMM_WORLD);
    MPI_Reduce(&t_allreduce, &max_allreduce, 1, MPI_DOUBLE, MPI_MAX, 0, MPI_COMM_WORLD);
    MPI_Reduce(&t_gather,    &max_gather,    1, MPI_DOUBLE, MPI_MAX, 0, MPI_COMM_WORLD);
    MPI_Reduce(&t_bcast,     &max_bcast,     1, MPI_DOUBLE, MPI_MAX, 0, MPI_COMM_WORLD);

    /* ----------------------------------------------------------
     * PHASE 5: PRINT RESULTS (rank 0)
     * ---------------------------------------------------------- */
    if (rank == 0) {
        double t_comm = max_scatter + max_reduce + max_allreduce
                      + max_gather + max_bcast;
        double speedup    = (t_serial > 0) ? t_serial / t_parallel : 0;
        double efficiency = (size > 0)     ? speedup / size         : 0;

        printf("\n");
        printf("============================================================\n");
        printf("  Q3: PERFORMANCE ANALYSIS - PDC Project\n");
        printf("  File: %s\n", argv[1]);
        printf("  Records: %d  |  MPI Processes: %d\n", total_lines, size);
        printf("============================================================\n");

        /* --- Serial results --- */
        printf("\n--- SERIAL ANALYSIS (Baseline) ---\n");
        for (int i = 0; i < NUM_ATTACKS; i++)
            printf("  %-18s %d\n", ATK_NAMES[i], serial_counts[i]);
        printf("  %-18s %d\n", "Total attacks:",
               serial_counts[0] + serial_counts[1] + serial_counts[2]);
        printf("  %-18s %lu\n", "Checksum:", serial_checksum);
        printf("  %-18s %.6f s\n", "Serial time:", t_serial);

        /* --- Parallel results --- */
        printf("\n--- PARALLEL ANALYSIS (np=%d) ---\n", size);
        for (int i = 0; i < NUM_ATTACKS; i++)
            printf("  %-18s %d\n", ATK_NAMES[i], global_counts[i]);
        printf("  %-18s %d\n", "Total attacks:",
               global_counts[0] + global_counts[1] + global_counts[2]);
        printf("  %-18s %lu  %s\n", "Checksum:", global_checksum,
               (global_checksum == serial_checksum) ? "[VERIFIED]" : "[MISMATCH!]");

        /* --- Per-rank breakdown --- */
        printf("\n--- PER-RANK BREAKDOWN ---\n");
        printf("  %-6s  %-10s  %-10s  %-15s  %-8s\n",
               "Rank", "Backdoor", "DoS", "Reconnaissance", "Lines");
        for (int r = 0; r < size; r++) {
            int b  = gathered[r * NUM_ATTACKS + ATK_BACKDOOR];
            int d  = gathered[r * NUM_ATTACKS + ATK_DOS];
            int rc = gathered[r * NUM_ATTACKS + ATK_RECON];
            int lines_for_rank = chunk_size;
            if (r == size - 1 && padded_lines > total_lines)
                lines_for_rank = chunk_size - (padded_lines - total_lines);
            printf("  %-6d  %-10d  %-10d  %-15d  %-8d\n",
                   r, b, d, rc, lines_for_rank);
        }

        /* --- Timing breakdown --- */
        printf("\n--- TIMING BREAKDOWN (seconds) ---\n");
        printf("  File I/O:              %10.6f\n", t_file_read);
        printf("  Serial analysis:       %10.6f\n", t_serial);
        printf("  -----------------------------------------\n");
        printf("  MPI_Scatter:           %10.6f\n", max_scatter);
        printf("  Local computation:     %10.6f\n", max_compute);
        printf("  MPI_Reduce:            %10.6f\n", max_reduce);
        printf("  MPI_Allreduce:         %10.6f\n", max_allreduce);
        printf("  MPI_Gather:            %10.6f\n", max_gather);
        printf("  MPI_Bcast:             %10.6f\n", max_bcast);
        printf("  -----------------------------------------\n");
        printf("  Total communication:   %10.6f\n", t_comm);
        printf("  Total parallel:        %10.6f\n", t_parallel);
        printf("  Total program:         %10.6f\n", t_program);

        /* --- Performance metrics table --- */
        printf("\n--- PERFORMANCE METRICS ---\n");
        printf("  +------+------------+------------+---------+------------+----------+\n");
        printf("  | np   | T_serial   | T_parallel | Speedup | Efficiency | CommOvhd |\n");
        printf("  +------+------------+------------+---------+------------+----------+\n");
        printf("  | %-4d | %10.6f | %10.6f | %7.4f | %8.1f%%   | %6.1f%%   |\n",
               size, t_serial, t_parallel, speedup, efficiency * 100,
               (t_parallel > 0) ? (t_comm / t_parallel * 100) : 0);
        printf("  +------+------------+------------+---------+------------+----------+\n");

        /* --- Communication overhead discussion --- */
        printf("\n--- COMMUNICATION OVERHEAD ANALYSIS ---\n");
        printf("  Computation time:      %10.6f s (%.1f%% of parallel)\n",
               max_compute,
               (t_parallel > 0) ? max_compute / t_parallel * 100 : 0);
        printf("  Communication time:    %10.6f s (%.1f%% of parallel)\n",
               t_comm,
               (t_parallel > 0) ? t_comm / t_parallel * 100 : 0);
        printf("  Comp/Comm ratio:       %.2f : 1\n",
               (t_comm > 0) ? max_compute / t_comm : 0);

        if (t_comm > max_compute)
            printf("  >> Communication-bound: overhead exceeds computation.\n");
        else
            printf("  >> Computation-bound: good parallelization potential.\n");

        if (speedup < 1.0)
            printf("  >> Slowdown at np=%d: MPI overhead exceeds parallel gain.\n", size);
        else if (efficiency < 0.5)
            printf("  >> Low efficiency (%.0f%%): diminishing returns at np=%d.\n",
                   efficiency * 100, size);
        else
            printf("  >> Good efficiency (%.0f%%): parallelization is effective.\n",
                   efficiency * 100);

        printf("\n============================================================\n");

        /* --- Append to CSV results file for multi-run benchmarking --- */
        FILE *csv = fopen("q3-haseeb/benchmark_results.csv", "a");
        if (csv) {
            fseek(csv, 0, SEEK_END);
            if (ftell(csv) == 0)
                fprintf(csv, "np,records,t_serial,t_parallel,t_scatter,t_compute,"
                        "t_reduce,t_allreduce,t_gather,t_bcast,t_comm,"
                        "speedup,efficiency\n");
            fprintf(csv, "%d,%d,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%.4f,%.4f\n",
                    size, total_lines, t_serial, t_parallel,
                    max_scatter, max_compute, max_reduce, max_allreduce,
                    max_gather, max_bcast, t_comm, speedup, efficiency);
            fclose(csv);
        }

        free(gathered);
        free(all_lines);
    }

    free(local_lines);
    MPI_Finalize();
    return 0;
}
