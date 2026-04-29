# PDC Project Run Results

**Run date:** 2026-04-02  
**Source VM:** `139.84.171.89`  
**Project path on VM:** `/opt/pdc-project`  
**Execution mode:** Recompiled on VM, then executed with MPI

## Commands Used

```bash
cd /opt/pdc-project
mpicc -Wall -O2 q1-Lubna/main.c -o q1 -lm
mpicc -Wall -O2 q2-Insharah/main.c q2-Insharah/attack_detection.c -o q2 -lm
mpicc -Wall -O2 q3-haseeb/main.c -o q3 -lm

mpirun --allow-run-as-root --oversubscribe -np 4 ./q1 dataset/UNSW_NB15_training-set.csv
mpirun --allow-run-as-root --oversubscribe -np 4 ./q2
mpirun --allow-run-as-root --oversubscribe -np 4 ./q3 dataset/UNSW_NB15_training-set.csv
```

## Q1: Parallel Malicious Activity Detection

**Dataset:** `/opt/pdc-project/dataset/UNSW_NB15_training-set.csv`  
**Processes:** `4`  
**Records:** `82,332`

### Global Results

| Metric | Value |
|---|---:|
| Backdoor attacks | 583 |
| DoS attacks | 4,089 |
| Reconnaissance attacks | 3,496 |
| Total malicious records | 8,168 |
| Unique suspicious IPs | 2,606 |

### Per-Rank Breakdown

| Rank | Lines | Backdoor | DoS | Recon | Suspicious IPs |
|---|---:|---:|---:|---:|---:|
| 0 | 20,583 | 49 | 2,922 | 1,737 | 1,000 |
| 1 | 20,583 | 0 | 0 | 0 | 0 |
| 2 | 20,583 | 499 | 999 | 1,356 | 1,000 |
| 3 | 20,583 | 35 | 168 | 403 | 606 |

### Note

The aggregate attack counts are internally consistent and match Q3.  
The printed sample of flagged IPs in Q1 showed numeric-only values such as `244`, `245`, `246`, which suggests an output/parsing issue in the sample IP display path.

## Q2: Distributed Attack Detection and Validation

**Dataset:** `UNSW-NB15 raw CSV split across 4 files`  
**Processes:** `4`

### Summary Results

| Metric | Value |
|---|---:|
| Max suspicious IPs in one process | 24 |
| Total failed logins | 468,828 |
| Total port scans | 183,521 |
| Total connection attempts | 339,807 |
| Global suspicious IPs before deduplication | 94 |
| Unique suspicious IPs after deduplication | 36 |
| Validation status | PASSED |
| Verdict | POTENTIAL DDoS / PORT-SCANNING ATTACK DETECTED |

### Per-Process Suspicious IP Counts

| Process | Suspicious IPs |
|---|---:|
| 0 | 23 |
| 1 | 24 |
| 2 | 24 |
| 3 | 23 |

### Checksum Report

| Process | Checksum | Bytes |
|---|---|---:|
| 0 | `0x8D1D7816` | 284 |
| 1 | `0x769FBB6C` | 284 |
| 2 | `0x0BF0C5EA` | 279 |
| 3 | `0x9CC76A1F` | 280 |

**Global XOR checksum:** `0x6CB56C8F`

### Output File

Q2 also wrote:

```text
/tmp/q2_results.json
```

## Q3: Performance Analysis

**Dataset:** `dataset/UNSW_NB15_training-set.csv`  
**Processes:** `4`  
**Records:** `82,332`

### Attack Detection Results

| Metric | Value |
|---|---:|
| Backdoor attacks | 583 |
| DoS attacks | 4,089 |
| Reconnaissance attacks | 3,496 |
| Total attacks | 8,168 |
| Checksum | 3879152257442788879 |
| Checksum verification | VERIFIED |

### Per-Rank Breakdown

| Rank | Backdoor | DoS | Reconnaissance | Lines |
|---|---:|---:|---:|---:|
| 0 | 49 | 2,922 | 1,737 | 20,583 |
| 1 | 0 | 0 | 0 | 20,583 |
| 2 | 499 | 999 | 1,356 | 20,583 |
| 3 | 35 | 168 | 403 | 20,583 |

### Timing Breakdown

| Metric | Time (s) |
|---|---:|
| File I/O | 0.113768 |
| Serial analysis | 0.016589 |
| MPI_Scatter | 0.043667 |
| Local computation | 0.006039 |
| MPI_Reduce | 0.016154 |
| MPI_Allreduce | 0.009601 |
| MPI_Gather | 0.000011 |
| MPI_Bcast | 0.000011 |
| Total communication | 0.069443 |
| Total parallel | 0.033340 |
| Total program | 0.163751 |

### Performance Metrics

| np | T_serial (s) | T_parallel (s) | Speedup | Efficiency | Communication Overhead |
|---:|---:|---:|---:|---:|---:|
| 4 | 0.016589 | 0.033340 | 0.4976x | 12.4% | 208.3% |

### Interpretation

- The attack totals match Q1 exactly.
- The workload is communication-bound at `np=4`.
- MPI overhead exceeded the parallel compute gain in this run.

## Overall Conclusion

- Q1 and Q3 produced identical attack counts on the training dataset:
  - Backdoor: `583`
  - DoS: `4,089`
  - Reconnaissance: `3,496`
  - Total: `8,168`
- Q2 detected suspicious distributed behavior and passed checksum-based validation.
- The VM deployment is functioning and the binaries compile and execute successfully with MPI.
