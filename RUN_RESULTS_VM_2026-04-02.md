# PDC Project Results

## Question 1

### Parallel Malicious Activity Detection

| Metric | Value |
|---|---:|
| Records | 82,332 |
| Backdoor | 583 |
| DoS | 4,089 |
| Reconnaissance | 3,496 |
| Total malicious records | 8,168 |
| Unique suspicious IPs | 2,606 |

### Per-Rank Breakdown

| Rank | Lines | Backdoor | DoS | Reconnaissance | Suspicious IPs |
|---|---:|---:|---:|---:|---:|
| 0 | 20,583 | 49 | 2,922 | 1,737 | 1,000 |
| 1 | 20,583 | 0 | 0 | 0 | 0 |
| 2 | 20,583 | 499 | 999 | 1,356 | 1,000 |
| 3 | 20,583 | 35 | 168 | 403 | 606 |

## Question 2

### Distributed Attack Detection

| Metric | Value |
|---|---:|
| Unique suspicious IPs | 36 |
| Failed logins | 468,828 |
| Port scans | 183,521 |
| Connections | 339,807 |
| Validation | PASSED |
| Verdict | Potential DDoS / Port-Scanning Attack Detected |

### Per-Process Suspicious IP Counts

| Process | Suspicious IPs |
|---|---:|
| 0 | 23 |
| 1 | 24 |
| 2 | 24 |
| 3 | 23 |

## Question 3

### Attack Totals

| Metric | Value |
|---|---:|
| Records | 82,332 |
| Backdoor | 583 |
| DoS | 4,089 |
| Reconnaissance | 3,496 |
| Total attacks | 8,168 |

### Benchmark Results

| np | T_serial (s) | T_parallel (s) | Speedup | Efficiency | Comm Overhead | Compute Time |
|---:|---:|---:|---:|---:|---:|---:|
| 1 | 0.016969 | 0.044402 | 0.38x | 38.2% | 63.8% | 0.016030 |
| 2 | 0.033313 | 0.068503 | 0.49x | 24.3% | 127.0% | 0.015799 |
| 3 | 0.020483 | 0.062837 | 0.33x | 10.9% | 142.7% | 0.015415 |
| 4 | 0.033230 | 0.044674 | 0.74x | 18.6% | 181.4% | 0.013329 |
| 5 | 0.036810 | 0.044293 | 0.83x | 16.6% | 224.4% | 0.005903 |
| 6 | 0.034533 | 0.044630 | 0.77x | 12.9% | 257.8% | 0.002977 |
