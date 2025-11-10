# run: bash run_test.sh > /dev/null 2>&1 &
# Be very careful about this script, don't be stupid unless you are confident about your program
# if you are using srun then book it for proper time lap
# otherwise you are occupying resources for nothing

#!/usr/bin/env bash
set -e
ts=$(date +%Y%m%d-%H%M%S)
th=$(date +"%A, %B %d, %Y - %r")

mkdir -p logs

cd logs && rm *.txt && cd ..

echo "LEGENDS:"

echo -e "M: METHODS,\nR: RECORDS,\nPS: PAYLOAD_SIZE,\nW: WORKERS,\nDC: DISTRIBUTION_CAP,\nWT: WORKING_TIME,\nTT: TOTAL_TIME"
echo ""
echo "==> Process has been start, wait till it finishes <=="
echo ""
echo "Starting test run at $th" > logs/run_$ts.txt 2>&1
echo "" >> logs/run_$ts.txt 2>&1

    ./seq_sort 1M 256 1 4 >> logs/run_$ts.txt 2>&1
    ./verifier_ff ../data/rec_1M_256.bin >> logs/run_$ts.txt 2>&1

    ./seq_sort 5M 128 1 4 >> logs/run_$ts.txt 2>&1
    ./verifier_ff ../data/rec_5M_128.bin >> logs/run_$ts.txt 2>&1

    ./seq_sort 5M 256 1 4 >> logs/run_$ts.txt 2>&1
    ./verifier_ff ../data/rec_5M_256.bin >> logs/run_$ts.txt 2>&1

    ./seq_sort 10M 64 1 4 >> logs/run_$ts.txt 2>&1
    ./verifier_ff ../data/rec_10M_64.bin >> logs/run_$ts.txt 2>&1

    ./seq_sort 10M 128 1 4 >> logs/run_$ts.txt 2>&1
    ./verifier_ff ../data/rec_10M_128.bin >> logs/run_$ts.txt 2>&1

    ./seq_sort 100M 16 1 4 >> logs/run_$ts.txt 2>&1
    ./verifier_ff ../data/rec_100M_16.bin >> logs/run_$ts.txt 2>&1

    ./seq_sort 100M 32 1 4 >> logs/run_$ts.txt 2>&1
    ./verifier_ff ../data/rec_100M_32.bin >> logs/run_$ts.txt 2>&1

    ./seq_sort 100M 256 1 4 >> logs/run_$ts.txt 2>&1
    ./verifier_ff ../data/rec_100M_256.bin >> logs/run_$ts.txt 2>&1

th=$(date +"%A, %B %d, %Y - %r")
echo "All steps completed at $th" >> logs/run_$ts.txt 2>&1