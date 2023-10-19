#!/usr/bin/env bash
set -euo pipefail

function exec_sysbench {
	local name="$1"
	shift
	
    for i in {1..3}; do
        sudo sysbench "$@" > /dev/null
    done

    for i in {1..3}; do
        echo "--------------"
        echo "#$i - sudo sysbench $@"
        echo
        sudo /usr/src/hivemind/tools/perf/perf mem record sysbench "$@"
		sudo /usr/src/hivemind/tools/perf/perf mem report --sort=mem | tee "results/sysbench.$name.$i.txt"
		mv perf.data "results/sysbench.$name.$i.dat"
        echo "--------------"
    done
}

exec_sysbench memory80 --threads=80 --test=memory run
exec_sysbench mutex80 --threads=80 --test=mutex run
exec_sysbench threads80 --threads=80 --test=threads run

exec_sysbench memory20 --threads=20 --test=memory run
exec_sysbench mutex20 --threads=20 --test=mutex run
exec_sysbench threads20 --threads=20 --test=threads run

exec_sysbench memory5 --threads=5 --test=memory run
exec_sysbench mutex5 --threads=5 --test=mutex run
exec_sysbench threads5 --threads=5 --test=threads run

exec_sysbench memory1 --threads=1 --test=memory run
exec_sysbench mutex1 --threads=1 --test=mutex run
exec_sysbench threads1 --threads=1 --test=threads run
