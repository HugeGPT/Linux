#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/time.h>
#include <signal.h>
#include <err.h>
#include <fcntl.h>
#include <time.h>

// asmlinkage long sys_hijack_madvise(int is_on);
// See function `usage` (or execute this agent without parameters) for syscall documents
#define SYSCALL_HIJACK_MADVISE 336

void usage(FILE *out)
{
	fprintf(out, "./hijack_madvise {is_on}\n");
	fprintf(out, "\n");
	fprintf(out, "An agent for calling syscall 336 `sys_hijack_madvise` on Hivemind.\n");
	fprintf(out, "\n");
	fprintf(out, "long sys_hijack_madvise(int is_on);\n");
	fprintf(out, "\n");
	fprintf(out, "Effect:\n");
	fprintf(out, "    When on, let the system ignore all MADV_HUGEPAGE from qemu-system-x64.\n");
	fprintf(out, "\n");
	fprintf(out, "Params:\n");
	fprintf(out, "    is_on: Shall the hijack happen, either 0 (false) or 1 (true).\n");
	fprintf(out, "\n");
	fprintf(out, "Return:\n");
	fprintf(out, "    Echo is_on back on success. Otherwise, return the errno.\n");
	exit(out == stderr);
}

int main(int argc, char *argv[])
{
	char *end = NULL;
	int enabled = 0;

	if (argc < 1 || !strcmp(argv[1], "-h") || !strcmp(argv[1], "--help"))
		usage(stderr);

	enabled = strtol(argv[1], &end, 0);

	printf("IS_ON = %d\n---\n", enabled);

	long result = syscall(SYSCALL_HIJACK_MADVISE, enabled);

	if (result != 0 && result != 1) {
		printf("Invocation failed with error %d\n", errno);
		return 1;
	}

	printf("Invocation succeed with result %d\n", printf);

	return 0;
}
