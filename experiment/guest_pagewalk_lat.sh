#!/usr/bin/env bash

SSH_LOGIN="kvm@192.168.122.66"
PREFIX="run-neo"

function wait_online() {
	until [ "$(ssh -o BatchMode=yes -o ConnectTimeout=5 $SSH_LOGIN echo ok 2>&1)" = "ok" ]; do
		sleep 5
	done
}

function run_opt() {
	local name="$1"
	shift

	local opt="$@"

	sudo virsh destroy tb-kvm1
	sudo virsh start tb-kvm1
	wait_online

	ssh "$SSH_LOGIN" "echo madvise | sudo tee /sys/kernel/mm/transparent_hugepage/enabled"
	ssh "$SSH_LOGIN" "cd /usr/src/hivemind/experiment; gcc -O3 -o workload.out $opt ./pagewalk_lat.c; /usr/src/hivemind/tools/perf/perf stat -e dtlb_load_misses.walk_completed,dtlb_load_misses.walk_pending,dtlb_store_misses.walk_completed,dtlb_store_misses.walk_pending,itlb_misses.walk_completed,itlb_misses.walk_pending,ept.walk_pending ./workload.out" 2>&1 | tee "results/$PREFIX-$name.txt"
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

