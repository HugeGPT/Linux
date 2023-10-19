#ifndef LINUX_HIVEMIND_H
#define LINUX_HIVEMIND_H

#include <linux/spinlock.h>
#include <linux/mm_types.h>
#include <linux/gfp.h>

// basic constants

#define HVM_PAGE_GROUP_ORDER 0
#define HVM_PAGE_GROUP_SIZE (1 << HVM_PAGE_GROUP_ORDER)

#define HVM_HUGE_PAGE_ORDER HPAGE_PMD_ORDER
#define HVM_HUGE_PAGE_TOTAL (1 << HVM_HUGE_PAGE_ORDER)

// building blocks

enum hvm_demote_kind {
	HVM_DEMOTE_USER_DATA = 1,
	HVM_DEMOTE_PAGE_DATA = 2,
};

enum hvm_page_pool_kind {
	HVM_KERNEL,
	HVM_USER,
#ifdef CONFIG_HIGHPTE
	HVM_USER_HIGH,
#endif

	HVM_POOL_KINDS,
};

struct hvm_page_group {
	struct page *page;
	int head;
	int tail;
	int total;
};

struct hvm_page_pool {
	struct hvm_page_group group;

	spinlock_t spinlock;
	int gfp;
};

// symlinks

#define HVM_GROUP_INIT ((struct hvm_page_group) { 0 })
#define hivemind_alloc_page(mm, gfp) hivemind_alloc_pages(mm, gfp, 0)

// functions

// guest side

int hivemind_init(void);
bool hivemind_init_done(void);
struct page *hivemind_alloc_pages(struct mm_struct *mm, gfp_t gfp, int order);
void hivemind_notify_setting_event(void);

// host side

int demote_memory_region(struct vm_area_struct *vma, unsigned long start, unsigned long end);
void flush_tlb_all_cpu(void);

extern bool hvm_demote_user_enabled;
extern bool hvm_demote_page_enabled;
long request_demote(unsigned long pfn, int count);

#endif /* LINUX_HIVEMIND_H */
