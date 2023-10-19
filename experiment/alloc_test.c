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

#define SYSCALL_SHOW_PGTBL 335

/*
SYSCALL_DEFINE2(show_pgtbl, pid_t, task_id, uint64_t, identifier)
{
	struct page *new = alloc_pages(0x400dc0, 9);

	int i;
	for (i = 0; i < 512; i++) {
		__free_page(new + i);
	}
	return 1;
}
*/

int main(int argc, char *argv[])
{
	for (int i = 0; i < 100; i++) {
		syscall(SYSCALL_SHOW_PGTBL, 0, 0);
	}

	return 0;
}
