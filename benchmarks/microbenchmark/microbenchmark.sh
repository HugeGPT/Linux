#!/usr/bin/env bash

PREFIX="run-neo"

function run_opt() {
	local name="$1"
	shift

	local opt="$@"

	gcc -O3 -o workload.out $opt ./pagewalk_lat.c
	echo
	echo "opt: ${opt[@]}"
	sudo /usr/src/hivemind/tools/perf/perf mem record ./workload.out
	sudo /usr/src/hivemind/tools/perf/perf mem report --sort=mem | tee "results/$PREFIX-$name.log"
	mv perf.data "results/$PREFIX-$name.dat"
}

mkdir -p results
echo madvise | sudo tee /sys/kernel/mm/transparent_hugepage/enabled

for i in {1..5}; do
	run_opt "bptp-busp-seq-rd-$i" "-DUSE_DEMOTE_PAGE" "-DUSE_DEMOTE_USER" "-DRUN_SEQ" "-DMEASURE_READ"
	run_opt "bptp-busp-seq-wr-$i" "-DUSE_DEMOTE_PAGE" "-DUSE_DEMOTE_USER" "-DRUN_SEQ" "-DMEASURE_WRITE"
	run_opt "bptp-busp-seq-rw-$i" "-DUSE_DEMOTE_PAGE" "-DUSE_DEMOTE_USER" "-DRUN_SEQ" "-DMEASURE_READ" "-DMEASURE_WRITE"

	run_opt "bptp-busp-rand-rd-$i" "-DUSE_DEMOTE_PAGE" "-DUSE_DEMOTE_USER" "-DRUN_RAND" "-DMEASURE_READ"
	run_opt "bptp-busp-rand-wr-$i" "-DUSE_DEMOTE_PAGE" "-DUSE_DEMOTE_USER" "-DRUN_RAND" "-DMEASURE_WRITE"
	run_opt "bptp-busp-rand-rw-$i" "-DUSE_DEMOTE_PAGE" "-DUSE_DEMOTE_USER" "-DRUN_RAND" "-DMEASURE_READ" "-DMEASURE_WRITE"

	run_opt "bptp-husp-seq-rd-$i" "-DUSE_DEMOTE_PAGE" "-DRUN_SEQ" "-DMEASURE_READ"
	run_opt "bptp-husp-seq-wr-$i" "-DUSE_DEMOTE_PAGE" "-DRUN_SEQ" "-DMEASURE_WRITE"
	run_opt "bptp-husp-seq-rw-$i" "-DUSE_DEMOTE_PAGE" "-DRUN_SEQ" "-DMEASURE_READ" "-DMEASURE_WRITE"

	run_opt "bptp-husp-rand-rd-$i" "-DUSE_DEMOTE_PAGE" "-DRUN_RAND" "-DMEASURE_READ"
	run_opt "bptp-husp-rand-wr-$i" "-DUSE_DEMOTE_PAGE" "-DRUN_RAND" "-DMEASURE_WRITE"
	run_opt "bptp-husp-rand-rw-$i" "-DUSE_DEMOTE_PAGE" "-DRUN_RAND" "-DMEASURE_READ" "-DMEASURE_WRITE"

	run_opt "hptp-busp-seq-rd-$i" "-DUSE_DEMOTE_USER" "-DRUN_SEQ" "-DMEASURE_READ"
	run_opt "hptp-busp-seq-wr-$i" "-DUSE_DEMOTE_USER" "-DRUN_SEQ" "-DMEASURE_WRITE"
	run_opt "hptp-busp-seq-rw-$i" "-DUSE_DEMOTE_USER" "-DRUN_SEQ" "-DMEASURE_READ" "-DMEASURE_WRITE"

	run_opt "hptp-busp-rand-rd-$i" "-DUSE_DEMOTE_USER" "-DRUN_RAND" "-DMEASURE_READ"
	run_opt "hptp-busp-rand-wr-$i" "-DUSE_DEMOTE_USER" "-DRUN_RAND" "-DMEASURE_WRITE"
	run_opt "hptp-busp-rand-rw-$i" "-DUSE_DEMOTE_USER" "-DRUN_RAND" "-DMEASURE_READ" "-DMEASURE_WRITE"

	run_opt "hptp-husp-seq-rd-$i" "-DRUN_SEQ" "-DMEASURE_READ"
	run_opt "hptp-husp-seq-wr-$i" "-DRUN_SEQ" "-DMEASURE_WRITE"
	run_opt "hptp-husp-seq-rw-$i" "-DRUN_SEQ" "-DMEASURE_READ" "-DMEASURE_WRITE"

	run_opt "hptp-husp-rand-rd-$i" "-DRUN_RAND" "-DMEASURE_READ"
	run_opt "hptp-husp-rand-wr-$i" "-DRUN_RAND" "-DMEASURE_WRITE"
	run_opt "hptp-husp-rand-rw-$i" "-DRUN_RAND" "-DMEASURE_READ" "-DMEASURE_WRITE"
done

