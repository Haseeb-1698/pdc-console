#!/usr/bin/env python3
"""
PDC Dashboard Server - Flask with SSE streaming
Runs on port 8080.
"""

import json
import subprocess
import os
import time
from flask import Flask, request, Response, send_from_directory
from flask_cors import CORS

PORT = 8080
WEB_DIR = "/opt/pdc-project/dashboard"
PROJECT_DIR = "/opt/pdc-project"

app = Flask(__name__, static_folder=WEB_DIR)
CORS(app)

MPI_HOSTFILE = "/opt/pdc-project/hosts.txt"
MPI_BASE_CMD = f"mpirun --hostfile {MPI_HOSTFILE} --mca plm_rsh_agent ssh"

ALLOWED_COMMANDS = {
    "compile_q1":           "cd /opt/pdc-project && mpicc -Wall -O2 q1-Lubna/main.c -o q1 -lm 2>&1 && echo 'Compiled: q1-Lubna/main.c -> ./q1  [OK]'",
    "compile_q2":           "cd /opt/pdc-project && mpicc -Wall -O2 q2-Insharah/main.c q2-Insharah/attack_detection.c -o q2 -lm 2>&1 && echo 'Compiled: q2-Insharah/main.c + attack_detection.c -> ./q2  [OK]'",
    "compile_q3":           "cd /opt/pdc-project && mpicc -Wall -O2 q3-haseeb/main.c -o q3 -lm 2>&1 && echo 'Compiled: q3-haseeb/main.c -> ./q3  [OK]'",
    "run_q1_np4":           f"cd /opt/pdc-project && {MPI_BASE_CMD} -np 4 ./q1 dataset/UNSW_NB15_training-set.csv 2>&1",
    "run_q2_np4":           f"cd /opt/pdc-project && {MPI_BASE_CMD} -np 4 ./q2 2>&1",
    "run_q3_np1":           f"cd /opt/pdc-project && {MPI_BASE_CMD} -np 1 ./q3 dataset/UNSW_NB15_training-set.csv 2>&1",
    "run_q3_np2":           f"cd /opt/pdc-project && {MPI_BASE_CMD} -np 2 ./q3 dataset/UNSW_NB15_training-set.csv 2>&1",
    "run_q3_np4":           f"cd /opt/pdc-project && {MPI_BASE_CMD} -np 4 ./q3 dataset/UNSW_NB15_training-set.csv 2>&1",
    "run_q3_np6":           f"cd /opt/pdc-project && {MPI_BASE_CMD} -np 6 ./q3 dataset/UNSW_NB15_training-set.csv 2>&1",
    "run_q3_combined_np4":  f"cd /opt/pdc-project && {MPI_BASE_CMD} -np 4 ./q3 dataset/UNSW_NB15_combined.csv 2>&1",
    "run_q3_combined_np6":  f"cd /opt/pdc-project && {MPI_BASE_CMD} -np 6 ./q3 dataset/UNSW_NB15_combined.csv 2>&1",
    "dataset_head":         "head -5 /opt/pdc-project/dataset/UNSW_NB15_training-set.csv 2>&1",
    "dataset_attacks":      "cd /opt/pdc-project/dataset && python3 -c \"import csv; from collections import Counter; c=Counter(); f=open('UNSW_NB15_training-set.csv'); r=csv.DictReader(f); [c.update([row['attack_cat'].strip()]) for row in r]; [print(f'  {k}: {v}') for k,v in c.most_common()]\" 2>&1",
    "sysinfo":              "echo '=== CPU ===' && lscpu | grep -E 'Model name|CPU\\(s\\):' | head -2 && echo '=== Memory ===' && free -h | grep Mem && echo '=== Disk ===' && df -h / | tail -1 && echo '=== MPI ===' && mpirun --version 2>&1 | head -1 && echo '=== GCC ===' && gcc --version | head -1 && echo '=== Uptime ===' && uptime",
    "project_tree":         "tree -L 2 /opt/pdc-project/ --dirsfirst 2>/dev/null || find /opt/pdc-project/ -maxdepth 2 -not -path '*/.git/*' | head -30",
    "benchmark_csv":        "cat /opt/pdc-project/q3-haseeb/benchmark_results.csv 2>&1",
    "full_benchmark":       f"cd /opt/pdc-project && rm -f q3-haseeb/benchmark_results.csv && for np in 1 2 3 4 5 6; do {MPI_BASE_CMD} -np $np ./q3 dataset/UNSW_NB15_combined.csv 2>&1; done && echo '=== DONE ==='",
}


# ── Static file serving ────────────────────────────────────────────────────────

@app.route('/')
def index():
    return send_from_directory(WEB_DIR, 'index.html')

@app.route('/<path:filename>')
def static_files(filename):
    return send_from_directory(WEB_DIR, filename)


# ── API: status ────────────────────────────────────────────────────────────────

@app.route('/api/status')
def api_status():
    try:
        uptime   = subprocess.check_output("uptime -p", shell=True).decode().strip()
        disk     = subprocess.check_output("df -h / | tail -1 | awk '{print $3\"/\"$2\" (\"$5\" used)\"}'", shell=True).decode().strip()
        mem      = subprocess.check_output("free -h | grep Mem | awk '{print $3\"/\"$2}'", shell=True).decode().strip()
        cpu_load = subprocess.check_output("cat /proc/loadavg | awk '{print $1}'", shell=True).decode().strip()
        mpi_ver  = subprocess.check_output("mpirun --version 2>&1 | head -1", shell=True).decode().strip()
        users    = subprocess.check_output("who | wc -l", shell=True).decode().strip()
        return json.dumps({
            "status": "online", "uptime": uptime, "disk": disk,
            "memory": mem, "cpu_load": cpu_load, "mpi_version": mpi_ver,
            "active_users": users, "hostname": "pdc-project", "ip": "52.147.201.53"
        }), 200, {'Content-Type': 'application/json'}
    except Exception as e:
        return json.dumps({"status": "error", "error": str(e)}), 200, {'Content-Type': 'application/json'}


# ── API: who (live SSH sessions) ──────────────────────────────────────────────

@app.route('/api/who')
def api_who():
    import datetime
    try:
        out = subprocess.check_output("who", shell=True).decode().strip()
        users = []
        seen = set()
        now = datetime.datetime.now()
        for line in out.splitlines():
            if not line.strip():
                continue
            parts = line.split()
            if len(parts) < 2:
                continue
            user = parts[0]
            tty  = parts[1]
            key  = (user, tty)
            if key in seen:
                continue
            seen.add(key)
            # Parse login time: "who" outputs date+time like "2026-03-29 10:45"
            duration = ''
            try:
                if len(parts) >= 4:
                    login_dt = datetime.datetime.strptime(parts[2] + ' ' + parts[3], '%Y-%m-%d %H:%M')
                    diff = now - login_dt
                    mins = int(diff.total_seconds() // 60)
                    if mins < 1:
                        duration = 'just now'
                    elif mins < 60:
                        duration = f'{mins}m'
                    else:
                        h, m = divmod(mins, 60)
                        duration = f'{h}h {m}m' if m else f'{h}h'
            except Exception:
                duration = ''
            users.append({"user": user, "tty": tty, "duration": duration})
        return json.dumps({"users": users, "count": len(users)}), 200, {'Content-Type': 'application/json'}
    except Exception as e:
        return json.dumps({"users": [], "count": 0, "error": str(e)}), 200, {'Content-Type': 'application/json'}


# ── API: benchmark CSV ─────────────────────────────────────────────────────────

@app.route('/api/benchmark')
def api_benchmark():
    csv_path = os.path.join(PROJECT_DIR, "q3-haseeb/benchmark_results.csv")
    try:
        with open(csv_path) as f:
            lines = f.readlines()
        headers = lines[0].strip().split(',')
        rows = [dict(zip(headers, line.strip().split(','))) for line in lines[1:] if line.strip()]
        return json.dumps({"results": rows}), 200, {'Content-Type': 'application/json'}
    except Exception as e:
        return json.dumps({"error": str(e)}), 200, {'Content-Type': 'application/json'}


# ── API: dataset info ──────────────────────────────────────────────────────────

@app.route('/api/dataset')
def api_dataset():
    dataset_dir = os.path.join(PROJECT_DIR, "dataset")
    files = []
    try:
        for fname in sorted(os.listdir(dataset_dir)):
            if fname.endswith('.csv'):
                path = os.path.join(dataset_dir, fname)
                size = os.path.getsize(path)
                with open(path) as fh:
                    lines = sum(1 for _ in fh)
                files.append({"name": fname, "size_mb": round(size / 1048576, 1), "rows": lines})
    except Exception as e:
        return json.dumps({"error": str(e)}), 200, {'Content-Type': 'application/json'}
    return json.dumps({"files": files}), 200, {'Content-Type': 'application/json'}


# ── API: dynamic file tree + file reader (for editor page) ───────────────────

TEXT_EXTENSIONS = {
    '.c', '.h', '.cpp', '.py', '.sh', '.md', '.txt', '.json',
    '.html', '.css', '.js', '.yml', '.yaml', '.toml', '.ini',
    '.cfg', '.conf', '.makefile', '.mk', '.csv', ''  # '' = no extension (Makefile etc)
}
SKIP_DIRS  = {'.git', '__pycache__', 'node_modules'}
SKIP_FILES = {'.pyc', '.o', '.out', '.bin', '.so', '.a'}
MAX_FILE_PREVIEW_BYTES = 512 * 1024

def _is_text_file(path):
    _, ext = os.path.splitext(path)
    basename = os.path.basename(path)
    if ext.lower() in SKIP_FILES:
        return False
    if ext.lower() in TEXT_EXTENSIONS or basename in ('Makefile', 'Dockerfile', 'README'):
        return True
    return False

def _safe_project_path(path):
    """Ensure path is inside PROJECT_DIR and not in a skip dir."""
    real = os.path.realpath(path)
    if not real.startswith(os.path.realpath(PROJECT_DIR) + os.sep):
        return False
    parts = real.split(os.sep)
    for skip in SKIP_DIRS:
        if skip in parts:
            return False
    return True

@app.route('/api/files')
def api_files():
    """Return the project file tree for the editor."""
    tree = []
    try:
        for root, dirs, files in os.walk(PROJECT_DIR):
            # Skip hidden and unwanted dirs in-place
            dirs[:] = sorted([d for d in dirs if d not in SKIP_DIRS and not d.startswith('.')])
            rel_root = os.path.relpath(root, PROJECT_DIR)
            folder_files = []
            for fname in sorted(files):
                if fname.startswith('.'):
                    continue
                fpath = os.path.join(root, fname)
                if _is_text_file(fpath) and _safe_project_path(fpath):
                    _, ext = os.path.splitext(fname)
                    lang = ext.lstrip('.').lower() or 'txt'
                    if fname in ('Makefile', 'makefile'):
                        lang = 'make'
                    folder_files.append({"name": fname, "path": fpath, "lang": lang})
            if folder_files:
                dir_label = rel_root if rel_root != '.' else 'root'
                tree.append({"dir": dir_label, "files": folder_files})
    except Exception as e:
        return json.dumps({"error": str(e)}), 200, {'Content-Type': 'application/json'}
    return json.dumps({"tree": tree}), 200, {'Content-Type': 'application/json'}

@app.route('/api/file')
def api_file():
    path = request.args.get('path', '')
    if not path:
        return json.dumps({"error": "No path provided"}), 200, {'Content-Type': 'application/json'}
    if not _safe_project_path(path):
        return json.dumps({"error": "Path not allowed"}), 200, {'Content-Type': 'application/json'}
    if not _is_text_file(path):
        return json.dumps({"error": "Binary or excluded file"}), 200, {'Content-Type': 'application/json'}
    try:
        size = os.path.getsize(path)
        with open(path, 'r', errors='replace') as f:
            content = f.read(MAX_FILE_PREVIEW_BYTES)
        truncated = size > MAX_FILE_PREVIEW_BYTES
        if truncated:
            content += f"\n\n[Preview truncated at {MAX_FILE_PREVIEW_BYTES // 1024} KiB. Full file size: {round(size / 1048576, 1)} MB.]"
        return json.dumps({"content": content, "path": path, "truncated": truncated, "size": size}), 200, {'Content-Type': 'application/json'}
    except Exception as e:
        return json.dumps({"error": str(e)}), 200, {'Content-Type': 'application/json'}


# ── API: run (legacy, non-streaming) ──────────────────────────────────────────

@app.route('/api/run')
def api_run():
    cmd_key = request.args.get('cmd', '')
    if cmd_key not in ALLOWED_COMMANDS:
        return json.dumps({"error": f"Unknown command: {cmd_key}", "allowed": list(ALLOWED_COMMANDS.keys())}), 200, {'Content-Type': 'application/json'}
    try:
        result = subprocess.run(
            ALLOWED_COMMANDS[cmd_key], shell=True,
            capture_output=True, text=True, timeout=300, cwd=PROJECT_DIR
        )
        return json.dumps({
            "command": cmd_key,
            "output": result.stdout + result.stderr,
            "exit_code": result.returncode
        }), 200, {'Content-Type': 'application/json'}
    except subprocess.TimeoutExpired:
        return json.dumps({"command": cmd_key, "error": "Timed out (300s)"}), 200, {'Content-Type': 'application/json'}
    except Exception as e:
        return json.dumps({"command": cmd_key, "error": str(e)}), 200, {'Content-Type': 'application/json'}


# ── API: stream (SSE, real-time output) ───────────────────────────────────────

@app.route('/api/stream')
def api_stream():
    cmd_key = request.args.get('cmd', '')
    if cmd_key not in ALLOWED_COMMANDS:
        def err():
            yield f"data: {json.dumps({'error': 'Unknown command: ' + cmd_key})}\n\n"
            yield "data: {\"done\": true, \"exit_code\": 1}\n\n"
        return Response(err(), mimetype='text/event-stream')

    def generate():
        try:
            proc = subprocess.Popen(
                ALLOWED_COMMANDS[cmd_key],
                shell=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                text=True, bufsize=1, cwd=PROJECT_DIR
            )
            for line in proc.stdout:
                line = line.rstrip('\n')
                yield f"data: {json.dumps({'line': line})}\n\n"
            proc.wait()
            yield f"data: {json.dumps({'done': True, 'exit_code': proc.returncode})}\n\n"
        except Exception as e:
            yield f"data: {json.dumps({'error': str(e)})}\n\n"
            yield "data: {\"done\": true, \"exit_code\": 1}\n\n"

    return Response(generate(), mimetype='text/event-stream',
                    headers={'X-Accel-Buffering': 'no', 'Cache-Control': 'no-cache'})


# ── API: run-benchmark (POST) ──────────────────────────────────────────────────

@app.route('/api/run-benchmark', methods=['POST'])
def api_run_benchmark():
    np_val  = request.args.get('np', '4')
    dataset = request.args.get('dataset', 'dataset/UNSW_NB15_training-set.csv')
    try:
        np_int = int(np_val)
        if np_int < 1 or np_int > 6:
            return json.dumps({"error": "np must be 1-6"}), 200, {'Content-Type': 'application/json'}
        safe_datasets = [
            "dataset/UNSW_NB15_training-set.csv",
            "dataset/UNSW_NB15_testing-set.csv",
            "dataset/UNSW_NB15_combined.csv"
        ]
        if dataset not in safe_datasets:
            return json.dumps({"error": f"Invalid dataset. Use: {safe_datasets}"}), 200, {'Content-Type': 'application/json'}
        cmd = f"cd /opt/pdc-project && mpirun --allow-run-as-root --oversubscribe -np {np_int} ./q3 {dataset} 2>&1"
        result = subprocess.run(cmd, shell=True, capture_output=True, text=True, timeout=60)
        return json.dumps({"np": np_int, "dataset": dataset,
                           "output": result.stdout + result.stderr,
                           "exit_code": result.returncode}), 200, {'Content-Type': 'application/json'}
    except Exception as e:
        return json.dumps({"error": str(e)}), 200, {'Content-Type': 'application/json'}


# ── Main ───────────────────────────────────────────────────────────────────────

if __name__ == '__main__':
    os.makedirs(WEB_DIR, exist_ok=True)
    print(f">> PDC Dashboard running at http://52.147.201.53:{PORT}")
    print(f">> Serving from {WEB_DIR}")
    app.run(host='0.0.0.0', port=PORT, threaded=True)
