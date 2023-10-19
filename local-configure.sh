#!/usr/bin/env bash
set -Eeuo pipefail

enable_flags \
	KVM \
	HVM_GUEST \
	HVM_HOST \
	HVM_PARAVIRT \
	HVM_DEBUG

# enable_flags HVM_DEBUG_VERBOSE
