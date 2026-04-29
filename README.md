# PDC Console

![PDC Console](./dashboard/img1.png)

![C](https://img.shields.io/badge/C-MPI-blue)
![OpenMPI](https://img.shields.io/badge/OpenMPI-Distributed%20Computing-orange)
![Azure](https://img.shields.io/badge/Azure-3%20Node%20MPI-0078D4)
![Dataset](https://img.shields.io/badge/Dataset-UNSW--NB15-purple)
![Status](https://img.shields.io/badge/Status-Complete-brightgreen)

PDC Console is a Parallel and Distributed Computing research project for intrusion-detection analysis on the UNSW-NB15 dataset. It combines MPI-based C programs, a dashboard presentation layer, and a code explorer for reviewing the implementation, results, and benchmark evidence.

| Link | URL |
|---|---|
| Live dashboard | https://pdc.onichealth.com/ |
| Repository | https://github.com/Haseeb-1698/pdc-console |

![PDC Console Code Explorer](./dashboard/img2.png)

## Project Focus

| Question | Focus | Main MPI Concepts |
|---|---|---|
| Q1 | Parallel detection of Backdoor, DoS, and Reconnaissance records | `MPI_Bcast`, `MPI_Scatterv`, `MPI_Reduce`, `MPI_Gather` |
| Q2 | Suspicious-IP correlation, deduplication, and validation | `MPI_Scatter`, `MPI_Reduce`, `MPI_Allreduce`, `MPI_Gatherv`, `MPI_Bcast` |
| Q3 | Serial vs parallel timing, speedup, efficiency, and overhead | `MPI_Wtime`, reductions, checksum verification |

The project demonstrates correct distributed coordination first, then uses the measurements to explain why this specific workload did not benefit from parallel execution.

## Actual Azure MPI Setup

The project was evaluated on a real 3-node Azure cluster with one master node and two worker nodes. The cluster has since been deleted, but the setup is documented so the environment can be understood and reproduced.

![Azure 3-node topology](./dashboard/img/AzureExportedTemplate.png)

| Item | Value |
|---|---|
| Cloud provider | Microsoft Azure |
| Region | `eastus` |
| Resource group | `pdc-fyp-rg` |
| VM image | Ubuntu Linux |
| VM size | `Standard_D2s_v3` |
| MPI runtime | OpenMPI |
| Project path | `/opt/pdc-project` |
| Dashboard port | `8080` |

| Role | VM name | Public IP | Private IP | Purpose |
|---|---|---|---|---|
| Master | `pdc-master` | `52.147.201.53` | `10.0.0.4` | Dashboard host, compile/run coordinator, MPI launcher |
| Worker 1 | `pdc-worker1` | `20.120.99.8` | `10.0.0.5` | MPI worker process execution |
| Worker 2 | `pdc-worker2` | `20.172.177.198` | `10.0.0.6` | MPI worker process execution |

MPI hostfile used during the demo environment:

```text
10.0.0.4 slots=2
10.0.0.5 slots=2
10.0.0.6 slots=2
```

Sanity check used to prove ranks were launching across the full cluster:

```bash
mpirun --hostfile /opt/pdc-project/hosts.txt --mca plm_rsh_agent ssh -np 6 hostname | sort | uniq -c
```

Expected distribution:

```text
2 pdc-master
2 pdc-worker1
2 pdc-worker2
```

## Demo Flow

The presentation flow followed the same order used in the live dashboard:

1. Open the dashboard and verify dataset/result cards.
2. Compile all three question programs.
3. Run Q1 with four MPI processes.
4. Run Q2 with four MPI processes.
5. Run Q3 with `np=1`, `np=2`, `np=4`, and `np=6`.
6. Review benchmark CSV output and performance conclusions.

## Dataset

The project uses UNSW-NB15 network traffic data.

| File | Used For |
|---|---|
| `UNSW_NB15_training-set.csv` | Q1 and Q3 labelled attack counting |
| `UNSW_NB15_testing-set.csv` | supporting labelled dataset file |
| `UNSW_NB15_combined.csv` | larger combined benchmark input |
| `UNSW-NB15_1.csv` to `UNSW-NB15_4.csv` | Q2 distributed suspicious-IP correlation |

Q1 and Q3 focus on the selected labelled attack categories. Q2 uses split raw traffic files to demonstrate rank-level correlation, validation, variable-size gathering, and final broadcast of the authoritative suspicious-IP list.

## Results

### Q1 and Q3 Attack Totals

| Metric | Value |
|---|---:|
| Records | 82,332 |
| Backdoor | 583 |
| DoS | 4,089 |
| Reconnaissance | 3,496 |
| Total malicious records | 8,168 |
| Unique suspicious IPs in Q1 | 2,606 |

Rank 1 showed zero detections in one Q1 run because the UNSW-NB15 training file was not shuffled. Its contiguous slice mostly contained non-target labels. This was an important observation: equal row counts do not always produce equal useful work.

### Q2 Correlation Results

| Metric | Value |
|---|---:|
| Max suspicious IPs in one process | 24 |
| Global suspicious IPs before deduplication | 94 |
| Unique suspicious IPs after deduplication | 36 |
| Failed logins | 468,828 |
| Port scans | 183,521 |
| Connection attempts | 339,807 |
| Validation | PASSED |

Q2 used a checksum and per-process byte reports to verify that ranks processed distinct data segments before suspicious-IP lists were gathered, deduplicated, and broadcast.

### Q3 Performance Finding

The selected Backdoor, DoS, and Reconnaissance workload was communication-bound.

| Observation | Meaning |
|---|---|
| Useful computation was very small | The selected label-counting work was often too light to amortize MPI startup and communication cost. |
| MPI communication dominated | Scatter, reduce, gather, and synchronization overhead exceeded the useful computation. |
| Speedup stayed below 1x | Sequential processing was faster for this exact workload and data slice. |
| Correctness still held | Parallel checksums matched the serial baseline, so the slowdown was a performance property, not a correctness failure. |

The main lesson is that parallelization must match the workload. For this dataset and these three selected labels, sequential processing was more efficient. Results could change with larger data, heavier feature extraction, broader label selection, or model inference where each record requires more computation.

## Build and Run

This repository preserves the source code, dashboard, command flow, and research presentation assets. It can be reused as a UI and MPI workflow template, but anyone reproducing the project should prepare their own dataset placement, cluster configuration, result files, and presentation content.

Compile:

```bash
make clean
make
```

Run Q1:

```bash
mpirun -np 4 ./q1 dataset/UNSW_NB15_training-set.csv
```

Run Q2:

```bash
mpirun -np 4 ./q2
```

Run Q3:

```bash
mpirun -np 2 ./q3 dataset/UNSW_NB15_training-set.csv
```

Multi-node run example:

```bash
mpirun --hostfile /opt/pdc-project/hosts.txt --mca plm_rsh_agent ssh -np 4 ./q1 dataset/UNSW_NB15_training-set.csv
```

For a new cluster, make sure all nodes have the same project files, compiled binaries, dataset files, OpenMPI version, and passwordless inter-node SSH configured for MPI.

## Project Structure

```text
.
|-- dashboard/
|   |-- index.html
|   |-- editor.html
|   |-- file-index.json
|   |-- img1.png
|   |-- img2.png
|   `-- img/
|       `-- AzureExportedTemplate.png
|-- q1-Lubna/
|   `-- main.c
|-- q2-Insharah/
|   |-- main.c
|   |-- attack_detection.c
|   `-- attack_detection.h
|-- q3-haseeb/
|   |-- main.c
|   |-- benchmark.sh
|   `-- benchmark_results.csv
|-- shared/
|   |-- STATUS.md
|   `-- RUN_RESULTS_VM_2026-04-02.md
|-- PDC_3_NODE_SETUP_RUNBOOK.md
|-- RUN_RESULTS_VM_2026-04-02.md
|-- Makefile
`-- README.md
```

## Related Documentation

- 🖥️ [PDC 3-Node Setup Runbook](./PDC_3_NODE_SETUP_RUNBOOK.md): full Azure MPI setup, cluster topology, commands, dataset placement, and findings.
- 📊 [Run Results](./RUN_RESULTS_VM_2026-04-02.md): result tables and command outputs used by the dashboard.
- ✅ [Project Status](./shared/STATUS.md): question-wise project completion summary.

## Contribution

Made with ❤️ by:

- Lubna
- Insharah
- Haseeb
