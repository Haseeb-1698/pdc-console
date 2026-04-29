# NetShield MPI Console

![Project Banner Placeholder](./dashboard/img/AzureExportedTemplate.png)

![C](https://img.shields.io/badge/C-17-blue)
![MPI](https://img.shields.io/badge/MPI-OpenMPI-orange)
![Status](https://img.shields.io/badge/status-complete-brightgreen)
![License](https://img.shields.io/badge/license-MIT-lightgrey)

Parallel network attack analysis project with a dashboard + code explorer UI.

## Links

- Repository: https://github.com/Haseeb-1698/pdc-console
- Live: https://pdc.onichealth.com/

## What’s Included

- `q1-Lubna/` malicious activity detection
- `q2-Insharah/` distributed attack correlation + validation
- `q3-haseeb/` serial vs parallel performance analysis
- `dashboard/` static dashboard + editor pages

## Quick Run (Local Linux / VM)

```bash
make
mpirun -np 4 ./q1 dataset/UNSW_NB15_training-set.csv
mpirun -np 4 ./q2
mpirun -np 2 ./q3 dataset/UNSW_NB15_training-set.csv
```

## Web Hosting (Static)

The dashboard/editor can be hosted as static files (cPanel/Apache/Nginx).

- `index.html` shows project + terminal output (predefined command responses)
- `editor.html` loads source files from `file-index.json`

## Screenshots

![Dashboard Placeholder](./dashboard/img/AzureExportedTemplate.png)
![Editor Placeholder](./dashboard/img/AzureExportedTemplate.png)
