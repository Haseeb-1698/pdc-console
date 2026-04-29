# PDC Console: 3-Node MPI Deployment Notes

This document summarizes the actual cluster setup used during the project evaluation. It intentionally excludes private connection details, keys, public IPs, subscription identifiers, and account-specific commands.

## Cluster Design

The project was validated on a 3-node MPI environment:

- 1 master node
- 2 worker nodes
- Linux-based VM environment
- OpenMPI installed on all nodes
- Project files deployed consistently under the same path on each node
- Dataset files available locally on each node to avoid repeated network reads

Each node used the same compiled C binaries:

- `q1` for parallel malicious activity detection
- `q2` for distributed suspicious-IP correlation
- `q3` for serial vs parallel performance measurement

## Execution Model

The master node coordinated MPI runs using a hostfile. The hostfile assigned process slots across the master and worker nodes, allowing commands such as:

```bash
mpirun --hostfile hosts.txt -np 4 ./q1 dataset/UNSW_NB15_training-set.csv
mpirun --hostfile hosts.txt -np 4 ./q2
mpirun --hostfile hosts.txt -np 2 ./q3 dataset/UNSW_NB15_training-set.csv
```

The dashboard terminal originally triggered these same compile and run commands through a backend service. For public hosting, the UI preserves the same presentation flow while the source code remains available for users who want to reproduce the setup on their own cluster.

## Dataset

The project used the UNSW-NB15 intrusion-detection dataset.

Important files used by the implementation:

- `UNSW_NB15_training-set.csv`
- `UNSW_NB15_testing-set.csv`
- `UNSW_NB15_combined.csv`
- `UNSW-NB15_1.csv`
- `UNSW-NB15_2.csv`
- `UNSW-NB15_3.csv`
- `UNSW-NB15_4.csv`

The first three files are useful for Q1 and Q3 style analysis. The four split files are useful for Q2 because each MPI process can work on a shard and then combine suspicious-IP results through MPI collectives.

## Question Mapping

### Q1: Parallel Malicious Activity Detection

Q1 reads the UNSW-NB15 training dataset, splits rows across MPI ranks, and counts selected attack categories:

- Backdoor
- DoS
- Reconnaissance

The global result is built through MPI reduction.

### Q2: Suspicious-IP Correlation

Q2 demonstrates cross-process suspicious-IP analysis. It uses MPI communication patterns to distribute work, aggregate statistics, validate processing, gather suspicious IPs, deduplicate them, and broadcast the final list.

MPI concepts used:

- `MPI_Scatter`
- `MPI_Reduce`
- `MPI_Allreduce`
- `MPI_Gather`
- `MPI_Gatherv`
- `MPI_Bcast`

### Q3: Performance Analysis

Q3 compares serial and parallel execution. It measures:

- serial analysis time
- parallel processing time
- communication overhead
- speedup
- efficiency
- checksum consistency

## Main Finding

The selected labels, especially Backdoor, DoS, and Reconnaissance, represented a relatively small portion of the workload being counted. The per-rank computation was very small, often in the microsecond range, while MPI communication and synchronization overhead was much larger.

Because of that, the parallel version did not outperform the sequential baseline for this specific task and dataset slice. In this case, a sequential implementation was more efficient.

This result does not mean MPI is ineffective. It means the chosen work unit was too small for the cost of distributed coordination. If the project targeted heavier labels, larger transformations, more expensive feature extraction, larger datasets, or model-style computation, the parallel result could change.

## What This Repository Provides

This repository is a presentation and reproduction package for the project:

- MPI C source code for the three questions
- web dashboard used to present the analysis
- code explorer UI for reviewing source files
- result summaries from the evaluated runs
- deployment notes explaining the 3-node MPI approach

Anyone reusing this project should create their own dataset placement, cluster, hostfile, and run commands for their environment.
