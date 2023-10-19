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

#define KiB 1024
#define MiB (1024 * KiB)
#define GiB (1024 * MiB)
#define PgB (4 * KiB)
#define HpB (512 * PgB)

// asmlinkage long sys_notify_userdata(unsigned long start, size_t len, int behavior);
#define SYSCALL_NOTIFY_USERDATA 337

enum hvm_demote_kind {
	HVM_DEMOTE_USER_DATA = 1,
	HVM_DEMOTE_PAGE_DATA = 2,
};

void usage(const char *prog, FILE *out)
{
    fprintf(out, "notify api test invoker\n");
    fprintf(out, "just run the program, enter 1 twice, and check dmesg for bugs/warnings\n");
    exit(out == stderr);
}

int main(int argc, char *argv[])
{
    long long memsz = 128 * HpB;
    long long i = 0;
        int _;
    char *data;

    printf("allocate 0x%llx byte memory\n", memsz);
    data = (char *) aligned_alloc(HpB, memsz);

    if (!data || data == MAP_FAILED) {
        perror("data mmap failure");
        exit(-1);
    }

    madvise(data, memsz, MADV_HUGEPAGE);

    memset(data, 0, memsz);

    printf("Allocation Complete ");
    scanf("%d", &_);

    syscall(SYSCALL_NOTIFY_USERDATA, (unsigned long) data, memsz, HVM_DEMOTE_USER_DATA);

    printf("Demote Complete ");
    scanf("%d", &_);

    free(data);
}
