# Be very careful about this script, don't be stupid unless you are confident about your program
# if you are using srun then book it for proper time lap
# otherwise you are occupying resources for nothing

#!/usr/bin/env bash
set -e
srun -n 1 make
ts=$(date +%Y%m%d-%H%M%S)
mkdir -p logs

echo "Starting sequential test run at $ts"
# sequential runs separately
./src/sequential 1M 256 1 32 > logs/seq_1M_256_$ts.txt 2>&1
./src/openmp 5M 128 1 32 > logs/op_5M_128_$ts.txt 2>&1
./src/farm 10M 32 1 32 > logs/farm_10M_32_$ts.txt 2>&1

echo "All 3 steps completed."