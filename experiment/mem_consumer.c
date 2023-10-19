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

void usage(const char *prog, FILE *out)
{
    fprintf(out, "usage: %s allocsize\n", prog);
    fprintf(out, " allocsize is kbytes, or number[KMGPH] (P = pages, H = hpages)\n");
    exit(out == stderr);
}

int main(int argc, char *argv[])
{
    long long memsz = 0;
    long long i = 0;
        int _;
    char *data;

    printf("ARG = %s\n", argv[1]);

    if (argc < 2)
        usage(argv[0], stderr);

    if (argc >= 2) {
        char *end = NULL;

        memsz = strtoull(argv[1], &end, 0);

        switch(*end) {
            case 'g':
            case 'G':
                printf("GiB = %lld\n", memsz);
                memsz *= GiB;
                break;
            case 'm':
            case 'M':
                printf("MiB = %lld\n", memsz);
                memsz *= MiB;
                break;
            case '\0':
            case 'k':
            case 'K':
                printf("KiB = %lld\n", memsz);
                memsz *= KiB;
                break;
            case 'h':
            case 'H':
                printf("HPg = %lld\n", memsz);
                memsz *= HpB;
                break;
            case 'p':
            case 'P':
                printf("BPg = %lld\n", memsz);
                memsz *= PgB;
                break;
            default:
                usage(argv[0], stderr);
                break;
        }

        if (memsz <= 0)
            usage(argv[0], stderr);
    }

    printf("allocate 0x%llx byte memory\n", memsz);
    data = (char *) mmap(0, memsz,
            PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);

    if (!data || data == MAP_FAILED) {
        perror("data mmap failure");
        exit(-1);
    }

    for (i = 0; i < memsz; i += KiB) {
        data[i]++;
    }

    printf("Allocation Complete ");
    scanf("%d", &_);

    munmap(data, memsz);
}
