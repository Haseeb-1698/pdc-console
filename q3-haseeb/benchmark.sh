#!/bin/bash
# Q3 Benchmark Script - Runs performance analysis with varying process counts
# Usage: bash benchmark.sh [csv_file]
# Default: dataset/UNSW_NB15_training-set.csv

cd /opt/pdc-project

DATASET=${1:-"dataset/UNSW_NB15_training-set.csv"}

echo "============================================================"
echo "  Q3 BENCHMARK - Running with np = 1, 2, 3, 4, 5, 6"
echo "  Dataset: $DATASET"
echo "============================================================"

# Clean previous results
rm -f q3-haseeb/benchmark_results.csv

# Compile
echo ""
echo "[*] Compiling Q3..."
mpicc -Wall -O2 q3-haseeb/main.c -o q3 -lm
if [ $? -ne 0 ]; then
    echo "[ERROR] Compilation failed!"
    exit 1
fi
echo "[OK] Compilation successful"

# Run benchmarks
for np in 1 2 3 4 5 6; do
    echo ""
    echo "============================================================"
    echo "  RUNNING WITH np = $np"
    echo "============================================================"
    mpirun --oversubscribe -np $np ./q3 "$DATASET"
    echo ""
done

# Print summary table from CSV
echo ""
echo "============================================================"
echo "  BENCHMARK SUMMARY TABLE"
echo "============================================================"
echo ""
if [ -f q3-haseeb/benchmark_results.csv ]; then
    printf "  %-4s  %-12s  %-12s  %-9s  %-12s  %-9s\n" \
           "np" "T_serial" "T_parallel" "Speedup" "Efficiency" "CommOvhd%"
    printf "  %-4s  %-12s  %-12s  %-9s  %-12s  %-9s\n" \
           "----" "----------" "----------" "-------" "----------" "-------"
    tail -n +2 q3-haseeb/benchmark_results.csv | while IFS=',' read -r np rec ts tp tsc tco tre tar tga tbr tcomm sp eff; do
        comm_pct=$(echo "scale=1; $tcomm / $tp * 100" | bc 2>/dev/null)
        eff_pct=$(echo "scale=1; $eff * 100" | bc 2>/dev/null)
        printf "  %-4s  %-12s  %-12s  %-9s  %-10s%%  %-7s%%\n" \
               "$np" "$ts" "$tp" "$sp" "$eff_pct" "$comm_pct"
    done
    echo ""
    echo "  Results saved to: q3-haseeb/benchmark_results.csv"
fi
echo "============================================================"
