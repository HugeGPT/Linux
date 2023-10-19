#!/usr/bin/env bash
set -Eeuo pipefail

echo 'Hivemind Prerequisite:'

check_arch "x86_64"

echo

echo 'Hivemind Support (at least one):'

check_flags \
	HVM_GUEST

EXITCODE_GUEST=${EXITCODE}
EXITCODE=0

check_flags \
	HVM_HOST

EXITCODE_HOST=${EXITCODE}
EXITCODE=0

if [ $EXITCODE_GUEST -ne 0 ] && [ $EXITCODE_HOST -ne 0 ]; then
	EXITCODE=1
fi

echo

echo 'Hivemind Paravirt:'

check_yes_flags \
	KVM

check_flags \
	HVM_PARAVIRT

echo

CODE=${EXITCODE}
EXITCODE=0

echo 'Hivemind Optional:'

check_flags \
	HVM_DEBUG \
	HVM_DEBUG_VERBOSE

echo

EXITCODE=${CODE}
