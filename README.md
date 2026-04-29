# PDC Console

![PDC Console Dashboard](./dashboard/img1.png)

![C](https://img.shields.io/badge/C-MPI-blue)
![OpenMPI](https://img.shields.io/badge/OpenMPI-Distributed%20Compute-orange)
![Dataset](https://img.shields.io/badge/Dataset-UNSW--NB15-purple)
![Status](https://img.shields.io/badge/Status-Complete-brightgreen)

PDC Console is a parallel network-traffic analysis project built for the PDC Spring 2026 semester project. It combines MPI-based C programs with a web dashboard and source-code explorer to present attack detection, distributed correlation, and performance benchmarking results.

## Live Project

- Live dashboard: https://pdc.onichealth.com/
- Repository: https://github.com/Haseeb-1698/pdc-console

## Overview

The project analyzes UNSW-NB15 network traffic data and focuses on three core parallel-computing tasks:

- Q1: Parallel malicious activity detection for Backdoor, DoS, and Reconnaissance attacks.
- Q2: Distributed suspicious-IP correlation using MPI collectives and validation checks.
- Q3: Serial vs parallel performance analysis with speedup, efficiency, and communication-overhead metrics.

The web UI is prepared for static hosting, so it can be deployed on cPanel or any standard static web server while still preserving the terminal-style project walkthrough and code explorer experience.

## Screenshots

### Dashboard

![Dashboard](./dashboard/img1.png)

### Code Explorer

![Code Explorer](./dashboard/img2.png)

## Features

- MPI-based distributed processing in C.
- Q1 attack detection with rank-wise and global aggregation.
- Q2 suspicious-IP cross-checking with `MPI_Scatter`, `MPI_Reduce`, `MPI_Allreduce`, `MPI_Gather`, `MPI_Gatherv`, and `MPI_Bcast`.
- Q3 benchmarking for serial and parallel execution.
- Static dashboard with terminal-style command output.
- Static code explorer powered by `file-index.json`.
- cPanel-compatible deployment structure.

## Project Structure

```text
.
├── dashboard/
│   ├── index.html
│   ├── editor.html
│   ├── file-index.json
│   ├── img1.png
│   └── img2.png
├── q1-Lubna/
│   └── main.c
├── q2-Insharah/
│   ├── main.c
│   ├── attack_detection.c
│   └── attack_detection.h
├── q3-haseeb/
│   ├── main.c
│   ├── benchmark.sh
│   └── benchmark_results.csv
├── shared/
│   └── RUN_RESULTS_VM_2026-04-02.md
├── Makefile
├── RUN_RESULTS_VM_2026-04-02.md
└── README.md
```

## Build and Run

Requirements:

- Linux environment or VM
- GCC
- OpenMPI
- UNSW-NB15 dataset files placed under `dataset/`

Compile all programs:

```bash
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

## Key Results

Q1 and Q3 produced matching attack totals on the UNSW-NB15 training dataset:

| Metric | Value |
|---|---:|
| Records | 82,332 |
| Backdoor | 583 |
| DoS | 4,089 |
| Reconnaissance | 3,496 |
| Total attacks | 8,168 |

Q2 distributed correlation results:

| Metric | Value |
|---|---:|
| Unique suspicious IPs | 36 |
| Failed logins | 468,828 |
| Port scans | 183,521 |
| Connections | 339,807 |
| Validation | PASSED |

## Static Hosting

For cPanel deployment, upload the dashboard files into the hosting root:

```text
public_html/
├── index.html
├── editor.html
├── file-index.json
├── img1.png
├── img2.png
├── q1-Lubna/
├── q2-Insharah/
├── q3-haseeb/
└── shared/
```

Use:

- `dashboard/index.html` as `public_html/index.html`
- `dashboard/editor.html` as `public_html/editor.html`
- `dashboard/file-index.json` as `public_html/file-index.json`
- `dashboard/img1.png` and `dashboard/img2.png` in `public_html/`

## Notes

- The hosted dashboard is static and uses predefined terminal outputs for presentation.
- The source programs remain compile-ready for Linux/OpenMPI environments.
- The code explorer reads files through `file-index.json`, so no backend API is required for hosting.
