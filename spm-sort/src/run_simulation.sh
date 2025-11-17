# run: bash run_simulation.sh 1M_256 32 > /dev/null 2>&1 &
# Be very careful about this script, don't be stupid unless you are confident about your program
# if you are using srun then book it for proper time lap
# otherwise you are occupying resources for nothing

#!/usr/bin/env bash
set -e

PROGRAM=$1
MEMORY_CAP=$2

# Validate MEMORY_CAP
if [[ ! "$MEMORY_CAP" =~ ^[0-9]+$ ]]; then
    echo "Error: MEMORY_CAP must be an integer"
    echo "Usage: bash run_simulation.sh <MEMORY_CAP>"
    echo "Example: bash run_simulation.sh 1M_256 32"
    exit 1
fi

if [ "$MEMORY_CAP" -lt 1 ] || [ "$MEMORY_CAP" -gt 32 ]; then
    echo "Error: MEMORY_CAP must be between 1 and 32"
    echo "Provided value: $MEMORY_CAP"
    exit 1
fi

if [ "$PROGRAM" == "1M_256" ];then
    srun --nodes=1 --ntasks=1 --cpus-per-task=32 --time=0:10:00 bash run_1M_256.sh $MEMORY_CAP > /dev/null 2>&1 &
fi

if [ "$PROGRAM" == "5M_128" ];then
    srun --nodes=1 --ntasks=1 --cpus-per-task=32 --time=0:10:00 bash run_5M_128.sh $MEMORY_CAP > /dev/null 2>&1 &
fi

if [ "$PROGRAM" == "10M_64" ];then
    srun --nodes=1 --ntasks=1 --cpus-per-task=32 --time=0:10:00 bash run_10M_64.sh $MEMORY_CAP > /dev/null 2>&1 &
fi

if [ "$PROGRAM" == "100M_16" ];then
    srun --nodes=1 --ntasks=1 --cpus-per-task=32 --time=0:30:00 bash run_100M_16.sh $MEMORY_CAP > /dev/null 2>&1 &
fi

