#if 0
gcc -mrdrnd -O3 -o .__temp.out "$@" "$0" && ./.__temp.out
rm -f ./.__temp.out
exit
#endif

#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <immintrin.h>
#include <signal.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <x86intrin.h>
#include <sys/mman.h>
#include <sys/time.h>
#include <sys/types.h>

//
// compile options
//
// #define USE_THP
// #define USE_RDRAND
// #define USE_DEMOTE_USER
// #define USE_DEMOTE_PAGE

// #define RUN_SEQ
// #define RUN_RAND

// #define MEASURE_READ
// #define MEASURE_WRITE

// #define ENABLE_EARLY_PURGE
// #define ENABLE_LATE_PURGE

//
// hints
//
#define __force_inline __attribute__((__always_inline__)) inline
#define likely(x)		__builtin_expect(!!(x), 1)
#define unlikely(x)		__builtin_expect(!!(x), 0)

//
// Processor specific constants
//

#define TLB_SIZE 1536UL
#define CACHE_SIZE (20UL * 1024 * 1024)
#define CACHE_LINE_SIZE 64
#define PAGE_SHIFT 12
#define PMD_SHIFT 21

//
// Page table description
//

#define __AC(X,Y) (X##Y)
#define _AC(X,Y) __AC(X,Y)

#define PAGE_SIZE (_AC(1,UL) << PAGE_SHIFT)
#define PAGE_MASK (~(PAGE_SIZE-1))

#define PMD_PAGE_SIZE (_AC(1, UL) << PMD_SHIFT)
#define PMD_PAGE_MASK (~(PMD_PAGE_SIZE-1))

#define HPAGE_PMD_ORDER (HPAGE_PMD_SHIFT - PAGE_SHIFT)
#define HPAGE_PMD_NR (1 << HPAGE_PMD_ORDER)
#define HPAGE_PMD_SHIFT PMD_SHIFT
#define HPAGE_PMD_SIZE	((1UL) << HPAGE_PMD_SHIFT)
#define HPAGE_PMD_MASK	(~(HPAGE_PMD_SIZE - 1))

//
// Expirement region description
//

#define TLB_REACH (TLB_SIZE * HPAGE_PMD_SIZE * 8)
#define TLB_ALLOC_SIZE (TLB_REACH * 2)

#define EXP_RANGE_PAGE_COUNT (TLB_REACH / PAGE_SIZE)
#define EXP_RANGE_SEQ_HEAD(x) (x)
#define EXP_RANGE_RAND_HEAD(x) (x + TLB_REACH)

//
// Alignment helpers
//

#define __ALIGN_KERNEL(x, a)		__ALIGN_KERNEL_MASK(x, (typeof(x))(a) - 1)
#define __ALIGN_KERNEL_MASK(x, mask)	(((x) + (mask)) & ~(mask))
#define ALIGN(x, a)		__ALIGN_KERNEL((x), (a))
#define ALIGN_DOWN(x, a)	__ALIGN_KERNEL((x) - ((a) - 1), (a))
#define __ALIGN_MASK(x, mask)	__ALIGN_KERNEL_MASK((x), (mask))
#define PTR_ALIGN(p, a)		((typeof(p))ALIGN((unsigned long)(p), (a)))
#define PTR_ALIGN_DOWN(p, a)	((typeof(p))ALIGN_DOWN((unsigned long)(p), (a)))
#define IS_ALIGNED(x, a)		(((x) & ((typeof(x))(a) - 1)) == 0)

//
// system interop
//

// asmlinkage long sys_flush_process_tlb();
#define SYSCALL_FLUSH_PROCESS_TLB 339

// though named flush "process" tlb, this syscall actually
// do full TLB flush on both guest and host
static __force_inline long flush_system_tlb() {
	return syscall(SYSCALL_FLUSH_PROCESS_TLB);
}

// asmlinkage long sys_notify_userdata(unsigned long start, size_t len, int behavior);
#define SYSCALL_NOTIFY_USERDATA 337

enum hvm_demote_kind {
	HVM_DEMOTE_USER_DATA = 1,
	HVM_DEMOTE_PAGE_DATA = 2,
};

// though named notify "user" data, this syscall actually
// can notify both user data and page table
static __force_inline long notify_demote_range(char *data) {
	int32_t mode = 0;

#ifdef USE_DEMOTE_USER
	mode |= HVM_DEMOTE_USER_DATA;
#endif

#ifdef USE_DEMOTE_PAGE
	mode |= HVM_DEMOTE_PAGE_DATA;
#endif

	if (!mode)
		return 0;

	printf("Mode %d ", mode);
	fflush(stdout);

	return syscall(SYSCALL_NOTIFY_USERDATA, (unsigned long) data, TLB_ALLOC_SIZE, mode);
}

//
// Memory initializer
//

static inline char *create_test_region(bool isThp) {
	uint64_t current;
	char *mapping;

	mapping = (char *) mmap(NULL, TLB_ALLOC_SIZE, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	if (!mapping || (unsigned long) mapping == -1) {
		perror("mapping mmap failure");
		exit(-1);
	}

	if (isThp && madvise(mapping, TLB_ALLOC_SIZE, MADV_HUGEPAGE) < 0) {
		perror("madv HP");
		exit(-1);
	}

	for (current = 0; current < TLB_ALLOC_SIZE; current += HPAGE_PMD_SIZE) {
		memset(mapping + current, 0, HPAGE_PMD_SIZE);
	}

	return mapping;
}

static inline void free_test_region(char *p) {
	munmap(p, TLB_ALLOC_SIZE);
}

//
// Memory accessor
//

// https://en.wikipedia.org/wiki/Xorshift#xorshift*
static __force_inline uint64_t xorshift64star(void) {
	static uint64_t x = 42;
	x ^= x >> 12;
	x ^= x << 25;
	x ^= x >> 27;
	return x * 0x2545F4914F6CDD1DULL;
}

static __force_inline uint64_t generate_rand_real() {
	long long unsigned int rand = 0;
	int ret = 0;

	ret = _rdrand64_step(&rand);

	return rand;
}

static __force_inline uint64_t generate_rand_psudo() {
	return xorshift64star();
}

static __force_inline uint64_t precalc_thres(uint64_t one_past_max) {
	// find the largest number below UINT64_MAX which satisfies X % one_past_max == one_past_max - 1
	return UINT64_MAX - ((UINT64_MAX % one_past_max) + 1) % one_past_max;
}

static __force_inline uint64_t next_rand(uint64_t thres, uint64_t one_past_max) {
	uint64_t rand = 0;

	do {
#ifdef USE_RDRAND
		rand = generate_rand_real();
#else
		rand = generate_rand_psudo();
#endif

	} while(rand > thres);

	return rand % one_past_max;
}

static __force_inline void fisher_yates(char **arr, uint64_t n) {
	uint64_t i, j;
	char *tmp;

	for (i = n - 1; i > 0; i--) {
		j = next_rand(precalc_thres(i + 1), i + 1);
		tmp = arr[j];
		arr[j] = arr[i];
		arr[i] = tmp;
	}
}

static __force_inline char **create_seq_pg_impl(char *base) {
	uint64_t total_page = EXP_RANGE_PAGE_COUNT;
	uint64_t thres = precalc_thres(PAGE_SIZE);
	uint64_t i;

	char **list = (char **) malloc(total_page * sizeof(char *));

	for (i = 0; i < total_page; i++) {
		list[i] = base + i * PAGE_SIZE + next_rand(thres, PAGE_SIZE);
	}

	return list;
}

static __force_inline char **create_seq_pg_list(char *base) {
	return create_seq_pg_impl(EXP_RANGE_SEQ_HEAD(base));
}

static __force_inline char **create_rand_pg_list(char *base) {
	uint64_t total_page = EXP_RANGE_PAGE_COUNT;
	char **list = create_seq_pg_impl(EXP_RANGE_RAND_HEAD(base));

	fisher_yates(list, total_page);

	return list;
}

static __force_inline void free_pg_list(char **list) {
	free(list);
}

//
// Memory prober
//

static uint64_t measure_counter = 0;

static __force_inline uint64_t measure_read(char* addr) {
	uint64_t time1, time2, msl, hsl, osl;
	uintptr_t val = (uintptr_t) addr;
	uintptr_t aligned = val - val % 8;
	volatile uint64_t *ref = (volatile uint64_t *) aligned;
	volatile uint64_t temp = 0;

#ifdef ENABLE_LATE_PURGE
	if (unlikely(measure_counter % 512 == 0)) {
		flush_system_tlb();
	}

	measure_counter++;
#endif

	// initially the addr should not be in tlb
	_mm_mfence();
	_mm_clflush((void *)ref);
	_mm_mfence();

	_mm_lfence();
	time1 = __rdtsc();
	_mm_lfence();

#ifdef MEASURE_READ
	temp = *ref;
#endif
#ifdef MEASURE_WRITE
	*ref = temp + 1;
#endif

	_mm_mfence();
	_mm_lfence();
	time2 = __rdtsc();
	_mm_lfence();

	// page walk + fetch
	msl = time2 - time1;

	// now we have tlb for addr
	_mm_mfence();
	_mm_clflush((void *)ref);
	_mm_mfence();

	_mm_lfence();
	time1 = __rdtsc();
	_mm_lfence();

#ifdef MEASURE_READ
	temp = *ref;
#endif
#ifdef MEASURE_WRITE
	*ref = temp + 1;
#endif

	_mm_mfence();
	_mm_lfence();
	time2 = __rdtsc();
	_mm_lfence();

	// fetch time
	hsl = time2 - time1;

	osl = msl - hsl;

	if (osl > UINT64_MAX / 2)
		return UINT64_MAX;
	
	return osl;
}

static __force_inline uint64_t measure_all(char **page_list) {
	uint64_t time = 0;
	uint64_t succ = 0;
	uint64_t i;

#ifdef ENABLE_EARLY_PURGE
	flush_system_tlb();
#endif

	for (i = 0; i < EXP_RANGE_PAGE_COUNT; i++) {
		uint64_t measure = measure_read(page_list[i]);

		if (measure == UINT64_MAX)
			continue;

		succ++;
		time += measure;
	}

	return time / succ;
}

void main(void) {
	char *range;
	uint64_t i, seq, rand;
	char **page_list;
	bool use_thp = false;

#ifdef USE_THP
	use_thp = true;
#endif

	printf("TLB Size: %lu Records\n", TLB_SIZE);
	printf("TLB Reach: 0x%lx Bytes\n", TLB_REACH);
	printf("           %lu Pages\n", EXP_RANGE_PAGE_COUNT);

	printf("Allocating %s...", use_thp ? "THP" : "BP");
	fflush(stdout);
	range = create_test_region(use_thp);
	printf("Done\n");

	printf("Demoting...");
	fflush(stdout);
	notify_demote_range(range);
	printf("Done\n");

	for (i = 0; i < 3; i++) {
#ifdef RUN_RAND
		printf("Running Random Warmup (%lu of 3)...", i + 1);
		fflush(stdout);
		page_list = create_rand_pg_list(range);
		rand = measure_all(page_list);
		free_pg_list(page_list);
		printf("Reported %lu cycle/page walk\n", rand);
#endif

#ifdef RUN_SEQ
		printf("Running Sequence Warmup (%lu of 3)...", i + 1);
		fflush(stdout);
		page_list = create_seq_pg_list(range);
		seq = measure_all(page_list);
		free_pg_list(page_list);
		printf("Reported %lu cycle/page walk\n", seq);
#endif
	}

	for (i = 0; i < 3; i++) {
#ifdef RUN_RAND
		printf("Running Random Test (%lu of 3)...", i + 1);
		fflush(stdout);
		page_list = create_rand_pg_list(range);
		rand = measure_all(page_list);
		free_pg_list(page_list);
		printf("Reported %lu cycle/page walk\n", rand);
#endif

#ifdef RUN_SEQ
		printf("Running Sequence Test (%lu of 3)...", i + 1);
		fflush(stdout);
		page_list = create_seq_pg_list(range);
		seq = measure_all(page_list);
		free_pg_list(page_list);
		printf("Reported %lu cycle/page walk\n", seq);
#endif
	}

	printf("Freeing...");
	fflush(stdout);
	free_test_region(range);
	printf("Bye\n");
}
