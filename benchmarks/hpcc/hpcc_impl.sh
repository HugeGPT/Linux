#!/usr/bin/env bash
set -euo pipefail

if [ $# -ne 1 ]; then
    echo "./run.sh {nr_proc}"
    echo "    nr_proc: number of processes, no less than 4"
    exit
fi

NR_PROC="$1"

DATETIME=`date +"%FT%T"`

if [ -f "hpccoutf.txt" ]; then
    rm "hpccoutf.txt"
fi

../mpich/bin/mpirun -n "$NR_PROC" ./hpcc

cat "hpccoutf.txt"
mv "hpccoutf.txt" "../../output/hpccoutf.$NR_PROC.$DATETIME.txt"
