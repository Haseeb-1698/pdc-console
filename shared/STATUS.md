# PDC Project — Status (Updated: 2026-03-29)

## Team
| Member   | Question | Status        |
|----------|----------|---------------|
| Lubna    | Q1       | ✅ COMPLETE    |
| Insharah | Q2       | ✅ COMPLETE    |
| Haseeb   | Q3       | ✅ COMPLETE    |

---

## Q1 — Parallel Detection of Malicious Activities (Lubna)

**File:** `q1-Lubna/main.c`
**Compile:** `mpicc -Wall -O2 q1-Lubna/main.c -o q1 -lm`
**Run:** `mpirun --allow-run-as-root --oversubscribe -np 4 ./q1`

### MPI Operations Used
- MPI_Bcast — broadcast total line count to all processes
- MPI_Scatterv — distribute file chunks to each process
- MPI_Reduce — aggregate attack counts to master
- MPI_Allreduce — aggregate suspicious IP count across all ranks
- MPI_Gather — collect IP lists from all processes for deduplication
- MPI_Bcast — broadcast final results to all processes

### Results (UNSW_NB15_training-set.csv — 82,332 records, np=4)
| Attack Type    | Count |
|----------------|-------|
| Backdoor       | 583   |
| DoS            | 4,089 |
| Reconnaissance | 3,496 |
| **Total**      | **8,168** |
| Unique Suspicious IPs | 2,606 (deduplicated via MPI_Gather) |

---

## Q2 — Cross-Process Correlation and Error Checking (Insharah)

**Files:** `q2-Insharah/main.c`, `q2-Insharah/attack_detection.c`, `q2-Insharah/attack_detection.h`
**Compile:** `mpicc -Wall -O2 q2-Insharah/main.c q2-Insharah/attack_detection.c -o q2 -lm`
**Run:** `mpirun --allow-run-as-root --oversubscribe -np 4 ./q2`

### MPI Operations Used
- MPI_Scatter — distribute work counts to each process
- MPI_Reduce — aggregate statistics (MPI_MAX for suspicious, MPI_SUM for totals)
- MPI_Allreduce — global attack detection flags visible to all ranks
- MPI_Gather — collect per-process checksums for validation
- MPI_Gatherv — collect suspicious IP lists (variable size per process)
- MPI_Bcast — broadcast final deduplicated IP list to all processes

### Results (UNSW-NB15_1-4.csv — 4 raw files, np=4)
| Metric                  | Value   |
|-------------------------|---------|
| Unique Suspicious IPs   | 33      |
| Total Failed Logins     | 116,922 |
| Total Port Scans        | 46,153  |
| Total Connections       | 84,910  |
| Verdict                 | **DDoS / Port-Scanning DETECTED** |
| Validation Checksum     | PASSED  |

---

## Q3 — Performance Analysis (Haseeb)

**File:** `q3-haseeb/main.c`
**Compile:** `mpicc -Wall -O2 q3-haseeb/main.c -o q3 -lm`
**Run:** `mpirun --allow-run-as-root --oversubscribe -np <N> ./q3 dataset/UNSW_NB15_training-set.csv`

### MPI Operations Used
- MPI_Wtime — timing at every phase
- MPI_Bcast — broadcast line count and final results
- MPI_Scatter — distribute file chunks
- MPI_Reduce — aggregate attack counts + timing (MPI_MAX across ranks)
- MPI_Allreduce — global checksum verification + total attack count
- MPI_Gather — collect per-rank results

### Benchmark Results (82,332 records)
| np | T_serial (s) | T_parallel (s) | Speedup | Efficiency | Comm Overhead | Compute  |
|----|-------------|----------------|---------|------------|---------------|----------|
| 1  | 0.016969    | 0.044402       | 0.38x   | 38.2%      | 63.8%         | 0.016030s |
| 2  | 0.033313    | 0.068503       | 0.49x   | 24.3%      | 127.0%        | 0.015799s |
| 3  | 0.020483    | 0.062837       | 0.33x   | 10.9%      | 142.7%        | 0.015415s |
| 4  | 0.033230    | 0.044674       | 0.74x   | 18.6%      | 181.4%        | 0.013329s |
| 5  | 0.036810    | 0.044293       | **0.83x** | 16.6%   | 224.4%        | 0.005903s |
| 6  | 0.034533    | 0.044630       | 0.77x   | 12.9%      | 257.8%        | 0.002977s |

### Key Findings
- **Communication-bound:** MPI_Scatter dominates (~70-80% of parallel time). 82K lines x 512 bytes = ~40MB distributed per run.
- **Best speedup at np=5: 0.83x** — diminishing returns after that due to comm overhead growth.
- **Compute scales near-perfectly:** drops from 16ms (np=1) to 3ms (np=6) — near-linear scaling.
- **Amdahl's Law:** Serial I/O fraction limits theoretical max speedup regardless of process count.

---

## Infrastructure
| Item           | Details                          |
|----------------|----------------------------------|
| VM             | Vultr — 139.84.171.89            |
| OS             | Ubuntu 22.04                     |
| MPI            | OpenMPI                          |
| Dataset        | UNSW-NB15 (training + testing + raw CSVs) |
| Dashboard      | Flask — http://139.84.171.89:8080 |
| Editor         | http://139.84.171.89:8080/editor.html |
| Service        | systemd: pdc-dashboard           |

## Quick Commands
```bash
# Recompile all
mpicc -Wall -O2 q1-Lubna/main.c -o q1 -lm
mpicc -Wall -O2 q2-Insharah/main.c q2-Insharah/attack_detection.c -o q2 -lm
mpicc -Wall -O2 q3-haseeb/main.c -o q3 -lm

# Run all
mpirun --allow-run-as-root --oversubscribe -np 4 ./q1
mpirun --allow-run-as-root --oversubscribe -np 4 ./q2
mpirun --allow-run-as-root --oversubscribe -np 4 ./q3 dataset/UNSW_NB15_training-set.csv

# Dashboard
systemctl status pdc-dashboard
systemctl restart pdc-dashboard
```
