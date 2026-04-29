# PDC 3-Node Azure Cluster Runbook

Last updated: 2026-04-27  
Subscription: `FYP (9c249230-639b-4833-a278-be93becc7b46)`  
Resource Group: `pdc-fyp-rg`  
Region: `eastus`

## 1) Cluster Topology

- Master VM: `pdc-master`
  - Public IP: `52.147.201.53`
  - Private IP: `10.0.0.4`
- Worker VM 1: `pdc-worker1`
  - Public IP: `20.120.99.8`
  - Private IP: `10.0.0.5`
- Worker VM 2: `pdc-worker2`
  - Public IP: `20.172.177.198`
  - Private IP: `10.0.0.6`
- VM Size (all): `Standard_D2s_v3`
- Project path (all nodes): `/opt/pdc-project`

## 2) Local SSH Access (Windows PowerShell)

Private key location:

```powershell
$key="$HOME\.ssh\MyLowCostVM_key.pem"
```

Connect to master:

```powershell
C:\Windows\System32\OpenSSH\ssh.exe -i $key azureuser@52.147.201.53
```

## 3) Inter-node Communication (MPI)

Master hostfile:

```text
/opt/pdc-project/hosts.txt
10.0.0.4 slots=2
10.0.0.5 slots=2
10.0.0.6 slots=2
```

Master SSH config (used by OpenMPI):

```text
~/.ssh/config
Host 10.0.0.5
  User azureuser
  IdentityFile ~/.ssh/cluster_key.pem
  StrictHostKeyChecking accept-new
Host 10.0.0.6
  User azureuser
  IdentityFile ~/.ssh/cluster_key.pem
  StrictHostKeyChecking accept-new
```

Quick multi-node MPI check from master:

```bash
mpirun --hostfile /opt/pdc-project/hosts.txt --mca plm_rsh_agent ssh -np 6 hostname | sort | uniq -c
```

Expected pattern:

- 2 processes on `pdc-master`
- 2 processes on `pdc-worker1`
- 2 processes on `pdc-worker2`

## 4) Dataset Layout

Dataset folder:

```text
/opt/pdc-project/dataset
```

Required files:

- `UNSW_NB15_training-set.csv`
- `UNSW_NB15_testing-set.csv`
- `UNSW_NB15_combined.csv`
- `UNSW-NB15_1.csv`
- `UNSW-NB15_2.csv`
- `UNSW-NB15_3.csv`
- `UNSW-NB15_4.csv`

Verify:

```bash
ls -lh /opt/pdc-project/dataset
```

## 5) Build and Run Commands (Master)

Compile:

```bash
cd /opt/pdc-project
make clean
make
```

Run Q1:

```bash
cd /opt/pdc-project
mpirun --hostfile /opt/pdc-project/hosts.txt --mca plm_rsh_agent ssh -np 4 ./q1 dataset/UNSW_NB15_training-set.csv
```

Run Q2:

```bash
cd /opt/pdc-project
mpirun --hostfile /opt/pdc-project/hosts.txt --mca plm_rsh_agent ssh -np 4 ./q2
```

Run Q3 (training):

```bash
cd /opt/pdc-project
mpirun --hostfile /opt/pdc-project/hosts.txt --mca plm_rsh_agent ssh -np 2 ./q3 dataset/UNSW_NB15_training-set.csv
mpirun --hostfile /opt/pdc-project/hosts.txt --mca plm_rsh_agent ssh -np 6 ./q3 dataset/UNSW_NB15_training-set.csv
```

Run Q3 (combined):

```bash
cd /opt/pdc-project
mpirun --hostfile /opt/pdc-project/hosts.txt --mca plm_rsh_agent ssh -np 6 ./q3 dataset/UNSW_NB15_combined.csv
```

## 6) Dashboard + Editor

URLs:

- Dashboard: `http://52.147.201.53:8080/`
- Editor: `http://52.147.201.53:8080/editor.html`

Backend:

- File: `/opt/pdc-project/dashboard/server.py`
- Port: `8080`

Start server:

```bash
nohup python3 /opt/pdc-project/dashboard/server.py >/tmp/pdc-dashboard.log 2>&1 </dev/null &
```

Check status:

```bash
curl -sS http://127.0.0.1:8080/api/status
tail -n 50 /tmp/pdc-dashboard.log
```

Notes:

- Dashboard run actions now use hostfile-based distributed MPI.
- Editor API includes CSV files and truncates large previews safely.

## 7) Azure Operations (Start/Stop/Deallocate)

Set subscription:

```powershell
az account set --subscription 9c249230-639b-4833-a278-be93becc7b46
```

Start all VMs:

```powershell
az vm start -g pdc-fyp-rg -n pdc-master
az vm start -g pdc-fyp-rg -n pdc-worker1
az vm start -g pdc-fyp-rg -n pdc-worker2
```

Deallocate all VMs (stops compute billing):

```powershell
az vm deallocate -g pdc-fyp-rg -n pdc-master
az vm deallocate -g pdc-fyp-rg -n pdc-worker1
az vm deallocate -g pdc-fyp-rg -n pdc-worker2
```

Check power states:

```powershell
az vm list -g pdc-fyp-rg -d --query "[].{name:name,power:powerState,publicIp:publicIps}" -o table
```

## 8) Cost Control Checklist

1. Always use `az vm deallocate` after demo (not just shutdown inside Linux).
2. Keep cluster down except demo windows.
3. Keep only master public IP if workers are accessed only via private network.
4. Keep datasets on disks; no need to redownload every run.
5. For long-term savings, consider Savings Plan/Reserved if usage becomes regular.

## 9) Troubleshooting

`Permission denied (publickey)` from local:

- Fix key ACL and use OpenSSH from `C:\Windows\System32\OpenSSH\ssh.exe`.

MPI launches only on master:

- Check `~/.ssh/config`, `~/.ssh/cluster_key.pem`, and `/opt/pdc-project/hosts.txt`.
- Re-test with:
  - `ssh -i ~/.ssh/cluster_key.pem azureuser@10.0.0.5 hostname`
  - `ssh -i ~/.ssh/cluster_key.pem azureuser@10.0.0.6 hostname`

Dashboard opens but commands fail:

- Verify binaries and datasets:
  - `ls -lh /opt/pdc-project/q1 /opt/pdc-project/q2 /opt/pdc-project/q3`
  - `ls -lh /opt/pdc-project/dataset`
- Verify API directly:
  - `curl -sS "http://127.0.0.1:8080/api/run?cmd=run_q1_np4"`

Editor doesn’t show dataset files:

- Ensure server is the updated version and restarted:
  - `python3 -m py_compile /opt/pdc-project/dashboard/server.py`
  - `pkill -f dashboard/server.py && nohup python3 /opt/pdc-project/dashboard/server.py >/tmp/pdc-dashboard.log 2>&1 </dev/null &`

