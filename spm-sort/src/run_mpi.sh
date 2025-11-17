# run: bash run_simulation.sh > /dev/null 2>&1 &
# srun --nodes=1 --ntasks=1 --cpus-per-task=32 --time=01:00:00 bash run_simulation.sh 32 > /dev/null 2>&1 &
# Be very careful about this script, don't be stupid unless you are confident about your program
# if you are using srun then book it for proper time lap
# otherwise you are occupying resources for nothing

#!/usr/bin/env bash
set -e

MEMORY_CAP=$1

# Validate MEMORY_CAP
if [[ ! "$MEMORY_CAP" =~ ^[0-9]+$ ]]; then
    echo "Error: MEMORY_CAP must be an integer"
    echo "Usage: bash run_simulation.sh <MEMORY_CAP>"
    echo "Example: bash run_simulation.sh 32"
    exit 1
fi

if [ "$MEMORY_CAP" -lt 1 ] || [ "$MEMORY_CAP" -gt 32 ]; then
    echo "Error: MEMORY_CAP must be between 1 and 32"
    echo "Provided value: $MEMORY_CAP"
    exit 1
fi

ts=$(date +%Y%m%d-%H%M%S)
th=$(date +"%A, %B %d, %Y - %r")

mkdir -p logs


echo "LEGENDS:"

echo -e "M: METHODS,\nR: RECORDS,\nPS: PAYLOAD_SIZE,\nW: WORKERS,\nDC: DISTRIBUTION_CAP,\nWT: WORKING_TIME,\nTT: TOTAL_TIME\n" >> logs/run_mpi_$ts.txt 2>&1
echo ""
echo "==> Process has been start, wait till it finishes <=="
echo ""
echo "Starting test run at $th" >> logs/run_mpi_$ts.txt 2>&1
echo "" >> logs/run_mpi_$ts.txt 2>&1
echo -e "Starting MPI+FARM implementation:\n" >> logs/run_mpi_$ts.txt 2>&1

{
    for WORKERS in 2 4 8 16 32;do
        ./farm 1M 256 $WORKERS $MEMORY_CAP >> logs/run_mpi_$ts.txt 2>&1
        ./verify ../data/rec_1M_256.bin >> logs/run_mpi_$ts.txt 2>&1
    done
}

th=$(date +"%A, %B %d, %Y - %r")
echo "All steps completed at $th" >> logs/run_mpi_$ts.txt 2>&1