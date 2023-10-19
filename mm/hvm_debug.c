#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/mm.h>
#include <linux/sched/mm.h>
#include <linux/mm_inline.h>
#include <linux/syscalls.h>
#include <linux/gfp.h>
#include <linux/hugetlb.h>
#include <uapi/linux/kvm_para.h>
#include <uapi/asm/kvm_para.h>
#include <linux/hivemind.h>

#ifdef CONFIG_HVM_HOST

int madvise_hijack_enabled = 0;

bool is_madvise_hijack_target(struct task_struct *task)
{
	char *comm = task->comm;
	int ret = strcmp(comm, "qemu-system-x86");

	return ret == 0;
}

#endif /* CONFIG_HVM_HOST */

SYSCALL_DEFINE1(hijack_madvise, int, is_on)
{
#ifdef CONFIG_HVM_HOST

	madvise_hijack_enabled = !!is_on;

	return madvise_hijack_enabled;

#else
	pr_warn("Unexpected call to hijack_madvise\n");
	return -EINVAL;
#endif
}

static unsigned long total_req_page_cnt = 0;
static unsigned long total_suc_page_cnt = 0;

SYSCALL_DEFINE2(demote_test, unsigned long, start, size_t, len_in)
{
#ifdef CONFIG_HVM_HOST

	struct vm_area_struct *vma = NULL;
	unsigned long end = start + len_in;
	unsigned long i = 0;
	int chk_aligned, fails;

	vma = find_vma(current->mm, start);
	if (vma == NULL) {
		pr_warn("vma null\n");
		return -EINVAL;
	}
	if (start < vma->vm_start || end > vma->vm_end) {
		pr_warn("vma out-of-bound: start %lx < %lx or end %lx > %lx\n", 
			start, vma->vm_start, end, vma->vm_end);
		
		return -EINVAL;
	}

	// translate user addr to pfn
	fails = demote_memory_region(vma, start, end);

#ifdef CONFIG_HVM_DEBUG
	pr_warn("demote failures: %d / %lu\n", fails, len_in / HPAGE_PMD_SIZE);

	chk_aligned = 0;

	for (i = start; i < end; i += HPAGE_PMD_SIZE) {
		struct page *hostpg = follow_page(vma, start, FOLL_MIGRATION | FOLL_REMOTE);
		if (hostpg && (hostpg == compound_head(hostpg)) && PageTransHuge(hostpg) && !PageHuge(hostpg)) {
			chk_aligned++;
			pr_warn("demote failed at hpage zone: %lx, va %lx\n", hostpg ? page_to_pfn(hostpg) : 0, i);
		}
	}

#ifdef CONFIG_HVM_DEBUG_VERBOSE
	pr_warn("demote check: fail %lld / all %llu\n", len_in / HPAGE_PMD_SIZE - chk_aligned, len_in / HPAGE_PMD_SIZE);
#endif

	total_req_page_cnt += len_in / HPAGE_PMD_SIZE;
	total_suc_page_cnt += chk_aligned;

	if (total_req_page_cnt % 100 == 0) {
		pr_warn("demote check summary: %lu / %lu\n", total_suc_page_cnt, total_req_page_cnt);
	}

#endif /* CONFIG_HVM_DEBUG */

	return 0;

#else /* CONFIG_HVM_HOST */
	pr_warn("Unexpected call to demote_test\n");
	return -EINVAL;
#endif
}

#ifdef CONFIG_HVM_GUEST

long request_demote(unsigned long pfn, int count)
{
	return kvm_hypercall2(KVM_HC_HVM_DEMOTE_HPAGE, ALIGN_DOWN(pfn, HPAGE_PMD_NR), ALIGN(count, HPAGE_PMD_NR));
}

#else /* CONFIG_HVM_GUEST */

long request_demote(unsigned long pfn, int count)
{
	return 0;
}

#endif /* CONFIG_HVM_GUEST */

#ifdef CONFIG_HVM_GUEST

static long demote_user_data(struct mm_struct *mm, unsigned long start, size_t len_in)
{
	struct vm_area_struct *vma = NULL;
	unsigned long end = start + len_in;
	unsigned long i = 0;

	vma = find_vma(mm, start);
	if (vma == NULL) {
		pr_warn("vma null\n");
		return -EINVAL;
	}
	if (start < vma->vm_start || end > vma->vm_end) {
		pr_warn("vma out-of-bound: start %lx < %lx or end %lx > %lx\n", 
			start, vma->vm_start, end, vma->vm_end);
		
		return -EINVAL;
	}

	// translate user addr to pfn
	for (i = start; i < end; i += PAGE_SIZE) {
		struct page *page = follow_page(vma, i, FOLL_MIGRATION | FOLL_REMOTE);

		if (!page)
			continue;
		
		request_demote(page_to_pfn(page), 1);
	}

	return 0;
}

static inline pmd_t *pmd_indexer(pud_t *pud, int index)
{
	return (pmd_t *)pud_page_vaddr(*pud) + index;
}

static void demote_pmd_tree(pud_t *pud)
{
	int i;

	for (i = 0; i < PTRS_PER_PMD; i++) {
		pmd_t *pmd = pmd_indexer(pud, i);
		unsigned long pfn;

		if (pmd_none(*pmd) || unlikely(pmd_bad(*pmd)))
			continue;

		pfn = pmd_pfn(*pmd);

		if (pfn)
			request_demote(pfn, 1);
	}
}

static inline pud_t *pud_indexer(p4d_t *p4d, int index)
{
	return (pud_t *)p4d_page_vaddr(*p4d) + index;
}

static void demote_pud_tree(p4d_t *p4d)
{
	int i;

	for (i = 0; i < PTRS_PER_PUD; i++) {
		pud_t *pud = pud_indexer(p4d, i);
		unsigned long pfn;

		if (pud_none(*pud) || unlikely(pud_bad(*pud)))
			continue;

		pfn = pud_pfn(*pud);

		if (pfn)
			request_demote(pfn, 1);

		demote_pmd_tree(pud);
	}
}

static inline p4d_t *p4d_indexer(pgd_t *pgd, int index)
{
	if (!pgtable_l5_enabled())
		return (p4d_t *)pgd;
	return (p4d_t *)pgd_page_vaddr(*pgd) + index;
}

static void demote_p4d_tree(pgd_t *pgd)
{
	int i;
	int limit = PTRS_PER_P4D;

		if (!pgtable_l5_enabled()) {
		// When Page Tabel Level 5 not enabled, P4D is the PGD under-the-hood
		limit = 1;
	}

	for (i = 0; i < limit; i++) {
		p4d_t *p4d = p4d_indexer(pgd, i);
		unsigned long pfn;

		if (p4d_none(*p4d) || unlikely(p4d_bad(*p4d)))
			continue;

		pfn = p4d_pfn(*p4d);

		if (pfn)
			request_demote(pfn, 1);

		demote_pud_tree(p4d);
	}
}

static inline pgd_t *pgd_indexer(struct mm_struct *mm, int index)
{
	return (mm->pgd + index);
}

static long demote_page_data(struct mm_struct *mm)
{
	int i;

	for (i = 0; i < PTRS_PER_PGD; i++) {
		pgd_t *pgd = pgd_indexer(mm, i);
		unsigned long pfn;

		// if (i >= PTRS_PER_PGD / 2)
		// 	break;

		if (pgd_none(*pgd) || unlikely(pgd_bad(*pgd)))
			continue;

		pfn = pgd_pfn(*pgd);

		if (pfn)
			request_demote(pfn, 1);

		demote_p4d_tree(pgd);
	}

	return 0;
}

#endif /* CONFIG_HVM_GUEST*/

SYSCALL_DEFINE3(notify_userdata, unsigned long, start, size_t, len_in, int, behavior)
{
#ifdef CONFIG_HVM_GUEST
	int ret = 0;

	if (!ret && (behavior & HVM_DEMOTE_USER_DATA) == HVM_DEMOTE_USER_DATA) {
		ret = demote_user_data(current->mm, start, len_in);
	}
	
	if (!ret && (behavior & HVM_DEMOTE_PAGE_DATA) == HVM_DEMOTE_PAGE_DATA) {
		ret = demote_page_data(current->mm);
	}

	return ret;

#else /* CONFIG_HVM_GUEST */
	pr_warn("Unexpected call to notify_userdata\n");
	return -EINVAL;
#endif
}

static void do_flush_tlb_all_cpu(void *info)
{
	count_vm_tlb_event(NR_TLB_REMOTE_FLUSH_RECEIVED);
	preempt_disable();
	__flush_tlb_all();
	preempt_enable();
}

void flush_tlb_all_cpu(void)
{
	count_vm_tlb_event(NR_TLB_REMOTE_FLUSH);
	on_each_cpu(do_flush_tlb_all_cpu, NULL, 1);
}

SYSCALL_DEFINE0(flush_process_tlb)
{
#ifdef CONFIG_HVM_GUEST
	kvm_hypercall0(KVM_HC_HVM_FLUSH_HOST);
#endif

	flush_tlb_all_cpu();

	return 1;
}

bool hvm_demote_user_enabled = false;
bool hvm_demote_page_enabled = false;

SYSCALL_DEFINE1(demote_setting, int, value)
{
	hvm_demote_user_enabled = ((value & HVM_DEMOTE_USER_DATA) == HVM_DEMOTE_USER_DATA);
	hvm_demote_page_enabled = ((value & HVM_DEMOTE_PAGE_DATA) == HVM_DEMOTE_PAGE_DATA);
	hivemind_notify_setting_event();

	return 1;
}
