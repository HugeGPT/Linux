#!/usr/bin/env bash
set -euo pipefail

OUT="environment.txt"

function exec {
	echo "> $@" >> "$OUT"
	echo "" >> "$OUT"
    eval "$@" >> "$OUT"
	echo "" >> "$OUT"
}

touch "$OUT"

exec sudo lsb_release -a
exec sudo lscpu
exec sudo lsmod
exec sudo lshw -c memory

