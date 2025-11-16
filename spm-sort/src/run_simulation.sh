# run: bash run_simulation.sh > /dev/null 2>&1 &
# Be very careful about this script, don't be stupid unless you are confident about your program
# if you are using srun then book it for proper time lap
# otherwise you are occupying resources for nothing

#!/usr/bin/env bash
set -e
ts=$(date +%Y%m%d-%H%M%S)
th=$(date +"%A, %B %d, %Y - %r")

mkdir -p logs

cd logs && rm -f *.txt && cd ..

echo "LEGENDS:"

echo -e "M: METHODS,\nR: RECORDS,\nPS: PAYLOAD_SIZE,\nW: WORKERS,\nDC: DISTRIBUTION_CAP,\nWT: WORKING_TIME,\nTT: TOTAL_TIME\n" >> logs/run_$ts.txt 2>&1
echo ""
echo "==> Process has been start, wait till it finishes <=="
echo ""
echo "Starting test run at $th" >> logs/run_$ts.txt 2>&1
echo "" >> logs/run_$ts.txt 2>&1
echo -e "Starting Sequential implementation:\n" >> logs/run_$ts.txt 2>&1
# pure sequential version
# {
#     ./sequential 1M 256 1 4 >> logs/run_$ts.txt 2>&1
#     ./verify ../data/rec_1M_256.bin >> logs/run_$ts.txt 2>&1

#     ./sequential 5M 128 1 4 >> logs/run_$ts.txt 2>&1
#     ./verify ../data/rec_5M_128.bin >> logs/run_$ts.txt 2>&1

#     ./sequential 5M 256 1 4 >> logs/run_$ts.txt 2>&1
#     ./verify ../data/rec_5M_256.bin >> logs/run_$ts.txt 2>&1

#     ./sequential 10M 64 1 4 >> logs/run_$ts.txt 2>&1
#     ./verify ../data/rec_10M_64.bin >> logs/run_$ts.txt 2>&1

#     ./sequential 10M 128 1 4 >> logs/run_$ts.txt 2>&1
#     ./verify ../data/rec_10M_128.bin >> logs/run_$ts.txt 2>&1

#     ./sequential 100M 16 1 4 >> logs/run_$ts.txt 2>&1
#     ./verify ../data/rec_100M_16.bin >> logs/run_$ts.txt 2>&1

#     ./sequential 100M 32 1 4 >> logs/run_$ts.txt 2>&1
#     ./verify ../data/rec_100M_32.bin >> logs/run_$ts.txt 2>&1

#     ./sequential 100M 128 1 4 >> logs/run_$ts.txt 2>&1
#     ./verify ../data/rec_100M_128.bin >> logs/run_$ts.txt 2>&1
# }
echo -e "Starting OpenMP implementation:\n" >> logs/run_$ts.txt 2>&1
{
    for WORKERS in 2 4 8 16 32 48;do
        ./openmp 1M 256 $WORKERS 4 >> logs/run_$ts.txt 2>&1
        ./verify ../data/rec_1M_256.bin >> logs/run_$ts.txt 2>&1

        ./openmp 5M 128 $WORKERS 4 >> logs/run_$ts.txt 2>&1
        ./verify ../data/rec_5M_128.bin >> logs/run_$ts.txt 2>&1

        ./openmp 5M 256 $WORKERS 4 >> logs/run_$ts.txt 2>&1
        ./verify ../data/rec_5M_256.bin >> logs/run_$ts.txt 2>&1

        ./openmp 10M 64 $WORKERS 4 >> logs/run_$ts.txt 2>&1
        ./verify ../data/rec_10M_64.bin >> logs/run_$ts.txt 2>&1

        ./openmp 10M 128 $WORKERS 4 >> logs/run_$ts.txt 2>&1
        ./verify ../data/rec_10M_128.bin >> logs/run_$ts.txt 2>&1

        ./openmp 100M 16 $WORKERS 4 >> logs/run_$ts.txt 2>&1
        ./verify ../data/rec_100M_16.bin >> logs/run_$ts.txt 2>&1

        ./openmp 100M 32 $WORKERS 4 >> logs/run_$ts.txt 2>&1
        ./verify ../data/rec_100M_32.bin >> logs/run_$ts.txt 2>&1

        if [ $WORKERS -eq 4 ]; then
            ./openmp 100M 128 $WORKERS 4 >> logs/run_$ts.txt 2>&1
            ./verify ../data/rec_100M_128.bin >> logs/run_$ts.txt 2>&1
        fi
    done
}

echo -e "Starting FastFlow FARM implementation:\n" >> logs/run_$ts.txt 2>&1
{
    for WORKERS in 2 4 8 16 32 48;do
        ./farm 1M 256 $WORKERS 4 >> logs/run_$ts.txt 2>&1
        ./verify ../data/rec_1M_256.bin >> logs/run_$ts.txt 2>&1

        ./farm 5M 128 $WORKERS 4 >> logs/run_$ts.txt 2>&1
        ./verify ../data/rec_5M_128.bin >> logs/run_$ts.txt 2>&1

        ./farm 5M 256 $WORKERS 4 >> logs/run_$ts.txt 2>&1
        ./verify ../data/rec_5M_256.bin >> logs/run_$ts.txt 2>&1

        ./farm 10M 64 $WORKERS 4 >> logs/run_$ts.txt 2>&1
        ./verify ../data/rec_10M_64.bin >> logs/run_$ts.txt 2>&1

        ./farm 10M 128 $WORKERS 4 >> logs/run_$ts.txt 2>&1
        ./verify ../data/rec_10M_128.bin >> logs/run_$ts.txt 2>&1

        ./farm 100M 16 $WORKERS 4 >> logs/run_$ts.txt 2>&1
        ./verify ../data/rec_100M_16.bin >> logs/run_$ts.txt 2>&1

        ./farm 100M 32 $WORKERS 4 >> logs/run_$ts.txt 2>&1
        ./verify ../data/rec_100M_32.bin >> logs/run_$ts.txt 2>&1

        if [ $WORKERS -eq 4 ]; then
            ./farm 100M 128 $WORKERS 4 >> logs/run_$ts.txt 2>&1
            ./verify ../data/rec_100M_128.bin >> logs/run_$ts.txt 2>&1
        fi
    done
}

th=$(date +"%A, %B %d, %Y - %r")
echo "All steps completed at $th" >> logs/run_$ts.txt 2>&1