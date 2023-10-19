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

// asmlinkage long sys_demote_setting(int value);
#define SYSCALL_DEMOTE_SETTING 340

enum hvm_demote_kind {
	HVM_DEMOTE_USER_DATA = 1,
	HVM_DEMOTE_PAGE_DATA = 2,
};

int main(int argc, char *argv[])
{
    int choice = 0;
    int ret = 0;

    printf("Select One:\n");
    printf("0. Huge User + Huge PageTable\n");
    printf("1. DEMOTE User + Huge PageTable\n");
    printf("2. Huge User + DEMOTE PageTable\n");
    printf("3. DEMOTE User + DEMOTE PageTable\n");
    printf("Choice: ");
    scanf("%d", &choice);

    ret = syscall(SYSCALL_DEMOTE_SETTING, choice);

    printf("Result: %d\n", ret);
}
