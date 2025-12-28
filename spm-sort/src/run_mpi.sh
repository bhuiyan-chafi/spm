# run: bash run_mpi.sh 32> /dev/null 2>&1 &
# Be very careful about this script, don't be stupid unless you are confident about your program
# if you are using srun then book it for proper time lap
# otherwise you are occupying resources for nothing

#!/usr/bin/env bash
set -e

MEMORY_CAP=$1

# Validate MEMORY_CAP
if [[ ! "$MEMORY_CAP" =~ ^[0-9]+$ ]]; then
    echo "Error: MEMORY_CAP must be an integer"
    echo "Usage: bash run_mpi.sh <MEMORY_CAP>"
    echo "Example: bash run_mpi.sh 32"
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
echo -e "M: METHODS,\nR: RECORDS,\nPS: PAYLOAD_SIZE,\nW: WORKERS,\nDC: DISTRIBUTION_CAP,\nWT: WORKING_TIME,\nTT: TOTAL_TIME\n" >> logs/run_mpi_$ts.txt 2>&1
echo ""
echo "==> Process has been start, wait till it finishes <=="
echo ""
echo "Starting test run at $th" >> logs/run_mpi_$ts.txt 2>&1
echo "" >> logs/run_mpi_$ts.txt 2>&1
echo -e "Starting MPI+FF implementation:\n" >> logs/run_mpi_$ts.txt 2>&1

{
    for NODES in 2 4 6 8;do
        echo "NODE: $NODES" >> logs/run_mpi_$ts.txt 2>&1
        for WORKERS in 8 16 32 48 64;do
            srun --nodes=$NODES --ntasks-per-node=1 --cpus-per-task=32 --time=00:15:00 --mpi=pmix ./mpiff 1M 256 $WORKERS $MEMORY_CAP >> logs/run_mpi_$ts.txt 2>&1
            ./verify ../data/rec_1M_256.bin >> logs/run_mpi_$ts.txt 2>&1

            srun --nodes=$NODES --ntasks-per-node=1 --cpus-per-task=32 --time=00:15:00 --mpi=pmix ./mpiff 5M 128 $WORKERS $MEMORY_CAP >> logs/run_mpi_$ts.txt 2>&1
            ./verify ../data/rec_5M_128.bin >> logs/run_mpi_$ts.txt 2>&1

            srun --nodes=$NODES --ntasks-per-node=1 --cpus-per-task=32 --time=00:15:00 --mpi=pmix ./mpiff 10M 64 $WORKERS $MEMORY_CAP >> logs/run_mpi_$ts.txt 2>&1
            ./verify ../data/rec_10M_64.bin >> logs/run_mpi_$ts.txt 2>&1
        done
    done
}

th=$(date +"%A, %B %d, %Y - %r")
echo "All steps completed at $th" >> logs/run_mpi_$ts.txt 2>&1