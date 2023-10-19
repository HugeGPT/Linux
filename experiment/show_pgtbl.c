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

// long sys_show_pgtbl(pid_t task_id, uint64_t identifier);
// See function `usage` (or execute this agent without parameters) for syscall documents
#define SYSCALL_SHOW_PGTBL 335

void usage(FILE *out)
{
	fprintf(out, "./show_pgtbl {task_id}\n");
	fprintf(out, "\n");
	fprintf(out, "An agent for calling syscall 335 `sys_show_pgtbl` on Hivemind.\n");
	fprintf(out, "This agent will automatically filter and display the results.\n");
	fprintf(out, "\n");
	fprintf(out, "long sys_show_pgtbl(pid_t task_id, uint64_t identifier);\n");
	fprintf(out, "\n");
	fprintf(out, "Effect:\n");
	fprintf(out, "    Write the pfns of page table pages of process {task_id} to the kernel ring buffer.\n");
	fprintf(out, "    Use `cat /var/log/kern.log | grep '\\[HVM-{identifier}]' | ...` to yield results.\n");
	fprintf(out, "\n");
	fprintf(out, "Params:\n");
	fprintf(out, "    task_id: The PID of the process to display the page table info.\n");
	fprintf(out, "    identifier: An arbitrary decimal number, to help you filter the dmesg log.\n");
	fprintf(out, "\n");
	fprintf(out, "Return:\n");
	fprintf(out, "    Return 0 on success. Otherwise, return the errno.\n");
	fprintf(out, "\n");
	fprintf(out, "Notes (Agent Only):\n");
	fprintf(out, "    1. To use this agent, you need to install Perl 5.\n");
	fprintf(out, "       The syscall 335 itself does not have this requirment.\n");
	fprintf(out, "    2. This agent will automatically generate {identifier}s.\n");
	fprintf(out, "       So you don't need to provide one.\n");
	exit(out == stderr);
}

int main(int argc, char *argv[])
{
	char *end = NULL;
	int pid = 0;
	uint64_t identifier = time(NULL);

	if (argc < 2 || !strcmp(argv[1], "-h") || !strcmp(argv[1], "--help"))
		usage(stderr);

	pid = strtol(argv[1], &end, 0);

	printf("PID = %d\nIDENT = %lu\n---\n", pid, identifier);

	long result = syscall(SYSCALL_SHOW_PGTBL, pid, identifier);

	if (result != 1) {
		printf("Invocation failed with error %d\n", errno);
		return 1;
	}

	// wait for buffer flush to disk
	sleep(1);

	char virt_dmesg[100];
	sprintf(virt_dmesg, "cat /var/log/kern.log | grep -a '\\[HVM-%lu]' | perl -pe 's/^.*\\[HVM-\\d+\\] //'", identifier);
	system(virt_dmesg);

	return 0;
}
