# PDC Console: 3-Node Azure MPI Environment

This file documents the actual 3-node environment used for the PDC Console project evaluation. The cluster has since been deleted, but the setup is preserved here so the deployment and research workflow are reproducible. Private key material and credential details are intentionally not included.

## Environment Summary

| Item | Value |
|---|---|
| Cloud provider | Microsoft Azure |
| Region | `eastus` |
| Resource group | `pdc-fyp-rg` |
| VM image | Ubuntu Linux |
| VM size | `Standard_D2s_v3` |
| MPI runtime | OpenMPI |
| Project path on VMs | `/opt/pdc-project` |
| Dashboard port | `8080` |

## Cluster Topology

| Role | VM name | Public IP | Private IP | Purpose |
|---|---|---|---|---|
| Master | `pdc-master` | `52.147.201.53` | `10.0.0.4` | Dashboard host, compile/run coordinator, MPI launcher |
| Worker 1 | `pdc-worker1` | `20.120.99.8` | `10.0.0.5` | MPI worker process execution |
| Worker 2 | `pdc-worker2` | `20.172.177.198` | `10.0.0.6` | MPI worker process execution |

The master node launched MPI programs across the workers through private IP communication. Public IPs were used for management and dashboard access during the demo window.

## Network and MPI Communication

The master used an MPI hostfile with two slots per VM:

```text
10.0.0.4 slots=2
10.0.0.5 slots=2
10.0.0.6 slots=2
```

The expected distribution for a six-process check was:

```text
2 pdc-master
2 pdc-worker1
2 pdc-worker2
```

Typical cluster sanity check:

```bash
mpirun --hostfile /opt/pdc-project/hosts.txt --mca plm_rsh_agent ssh -np 6 hostname | sort | uniq -c
```

This proved that MPI was not only running locally on the master; it was launching ranks across the full master-worker setup.

## Software Installed

Each node had the same project dependencies:

- `gcc`
- `make`
- `mpicc`
- `mpirun`
- OpenMPI runtime
- Python/Flask dependencies for the dashboard on the master
- Kaggle dataset tooling during setup

The same project tree was deployed to all nodes at:

```text
/opt/pdc-project
```

This avoided path mismatches when OpenMPI started worker processes remotely.

## Dataset Preparation

The project used UNSW-NB15 intrusion-detection data. Dataset files were prepared on all nodes so each process could access files locally.

Required dataset files:

```text
dataset/UNSW_NB15_training-set.csv
dataset/UNSW_NB15_testing-set.csv
dataset/UNSW_NB15_combined.csv
dataset/UNSW-NB15_1.csv
dataset/UNSW-NB15_2.csv
dataset/UNSW-NB15_3.csv
dataset/UNSW-NB15_4.csv
```

Q1 and Q3 used the labelled training/combined files. Q2 used the four split raw CSV files to demonstrate distributed suspicious-IP correlation.

## Build Commands Used

From the project root:

```bash
cd /opt/pdc-project
make clean
make
```

Equivalent manual compilation:

```bash
mpicc -Wall -O2 q1-Lubna/main.c -o q1 -lm
mpicc -Wall -O2 q2-Insharah/main.c q2-Insharah/attack_detection.c -o q2 -lm
mpicc -Wall -O2 q3-haseeb/main.c -o q3 -lm
```

## Run Commands Used

Q1:

```bash
mpirun --hostfile /opt/pdc-project/hosts.txt --mca plm_rsh_agent ssh -np 4 ./q1 dataset/UNSW_NB15_training-set.csv
```

Q2:

```bash
mpirun --hostfile /opt/pdc-project/hosts.txt --mca plm_rsh_agent ssh -np 4 ./q2
```

Q3:

```bash
mpirun --hostfile /opt/pdc-project/hosts.txt --mca plm_rsh_agent ssh -np 1 ./q3 dataset/UNSW_NB15_training-set.csv
mpirun --hostfile /opt/pdc-project/hosts.txt --mca plm_rsh_agent ssh -np 2 ./q3 dataset/UNSW_NB15_training-set.csv
mpirun --hostfile /opt/pdc-project/hosts.txt --mca plm_rsh_agent ssh -np 4 ./q3 dataset/UNSW_NB15_training-set.csv
mpirun --hostfile /opt/pdc-project/hosts.txt --mca plm_rsh_agent ssh -np 6 ./q3 dataset/UNSW_NB15_combined.csv
```

## Dashboard Role

The dashboard was hosted on the master VM and exposed the project flow:

- compile Q1, Q2, Q3
- run MPI commands
- show terminal-style output
- show dataset summaries
- show benchmark results
- open the code explorer

The original dashboard backend ran Flask on port `8080` and triggered compile/run commands on the live VM. The public site now preserves the same presentation and source-review experience after the cluster was removed.

## Question Workflows

### Q1: Parallel Malicious Activity Detection

Q1 split the UNSW-NB15 training records across MPI ranks and counted:

- Backdoor
- DoS
- Reconnaissance

Important MPI behavior:

- rank 0 read the CSV
- total line count was broadcast to all ranks
- data chunks were distributed with `MPI_Scatterv`
- attack counts were aggregated with `MPI_Reduce`
- suspicious IP lists were gathered and deduplicated

Observed Q1 totals:

| Metric | Value |
|---|---:|
| Records | 82,332 |
| Backdoor | 583 |
| DoS | 4,089 |
| Reconnaissance | 3,496 |
| Total malicious records | 8,168 |
| Unique suspicious IPs | 2,606 |

Rank 1 had zero detections in the shown output because the dataset was not shuffled and that contiguous chunk mostly contained non-target labels. This showed that equal row partitioning does not always mean equal useful work.

### Q2: Suspicious-IP Correlation

Q2 demonstrated cross-process communication and validation:

- `MPI_Scatter` distributed work limits
- each rank loaded and processed its assigned shard
- `MPI_Reduce` aggregated totals
- `MPI_Allreduce` made global attack status visible to all ranks
- checksums validated distinct rank processing
- `MPI_Gatherv` collected variable-size suspicious-IP lists
- `MPI_Bcast` shared the final deduplicated list

Final Q2 run values:

| Metric | Value |
|---|---:|
| Max suspicious IPs in one process | 24 |
| Global suspicious IPs before deduplication | 94 |
| Unique suspicious IPs after deduplication | 36 |
| Failed logins | 468,828 |
| Port scans | 183,521 |
| Connection attempts | 339,807 |
| Validation | PASSED |

### Q3: Performance Analysis

Q3 compared serial and parallel execution using `MPI_Wtime` and checksum verification.

Measured concepts:

- serial baseline
- parallel time
- per-rank work split
- communication overhead
- speedup
- efficiency
- checksum correctness

The key result was that the chosen workload was communication-bound. MPI overhead, especially scatter/reduce/synchronization cost, was larger than the useful computation for these target labels.

## Main Research Finding

The project showed that parallelization is not automatically faster. For our selected labels, Backdoor, DoS, and Reconnaissance, the actual computation per process was small. The useful work was often in the microsecond range, while MPI communication overhead was much larger.

Because of that, sequential processing was more efficient for this exact workload and dataset slice. The MPI implementation was correct and useful for demonstrating distributed coordination, but it was not performance-optimal for the chosen label-counting task.

The result could change if:

- the dataset were much larger
- the computation per record were heavier
- more labels or different labels were selected
- feature extraction or model inference were added
- I/O and rank-0 bottlenecks were redesigned

This was the central lesson: parallel design must match the workload, not just the dataset size.
