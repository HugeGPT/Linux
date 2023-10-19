#!/usr/bin/env bash
set -euo pipefail

cd "deploy/hpcc"

function exec_hpcc {
	local nr_thread="$1"

    for i in {1..3}; do
        ./run.sh "$nr_thread" > /dev/null
    done

    for i in {1..3}; do
        echo "--------------"
		echo "#$i - mpirun -n $nr_thread hpcc"
        echo
        ./run.sh "$nr_thread"
        echo "--------------"
    done
}

exec_hpcc 64
exec_hpcc 32
exec_hpcc 16
exec_hpcc 8
exec_hpcc 4
exec_hpcc 2
exec_hpcc 1
