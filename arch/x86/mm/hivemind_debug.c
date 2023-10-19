#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/mm.h>
#include <linux/sched/mm.h>
#include <linux/mm_inline.h>
#include <linux/syscalls.h>
#include <linux/gfp.h>
#include <linux/hugetlb.h>

#include <asm/tlb.h>
#include <asm/pgalloc.h>
#include <asm/fixmap.h>
#include <asm/mtrr.h>

// @return: may be null
struct mm_struct *get_mm_from_pid(pid_t pid, int checked)
{
	const struct cred *cred = current_cred(), *tcred;
	struct task_struct *task;
	struct mm_struct *mm;

	rcu_read_lock();
	task = pid ? find_task_by_vpid(pid) : current;
	if (!task) {
		rcu_read_unlock();
		return NULL;
	}
	get_task_struct(task);

	if (checked) {
		/*
		* Check if this process has the right to modify the specified
		* process. The right exists if the process has administrative
		* capabilities, superuser privileges or the same
		* userid as the target process.
		*/
		tcred = __task_cred(task);
		if (!uid_eq(cred->euid, tcred->suid) && !uid_eq(cred->euid, tcred->uid) &&
			!uid_eq(cred->uid,  tcred->suid) && !uid_eq(cred->uid,  tcred->uid) &&
			!capable(CAP_SYS_NICE)) {
			rcu_read_unlock();
			put_task_struct(task);
			pr_warn("Leave get_mm_from_pid, permission denied\n");
			return NULL;
		}
	}

	rcu_read_unlock();

	if (security_task_movememory(task)) {
		put_task_struct(task);
		pr_warn("Leave get_mm_from_pid, security task\n");
		return NULL;
	}

	mm = get_task_mm(task);
	put_task_struct(task);

	if (!mm) {
		pr_warn("Leave get_mm_from_pid, mm_struct not found\n");
		return NULL;
	}

	return mm;
}

static inline pte_t *pte_indexer_kernel(pmd_t *pmd, int index)
{
	return (pte_t *)pmd_page_vaddr(*pmd) + index;
}

static void print_pte_tree(pmd_t *pmd, uint64_t identifier)
{
	int i;

	for (i = 0; i < PTRS_PER_PTE; i++) {
		pte_t *pte = pte_indexer_kernel(pmd, i);
		unsigned long pfn;

		if (pte_none(*pte))
			continue;

		pfn = pte_pfn(*pte);

		if (pfn)
			pr_warn("[HVM-%llu] \t\t\t\tPMD->PTE[%d] at pfn %lx\n", identifier, i, pfn);
	}
}

static inline pmd_t *pmd_indexer(pud_t *pud, int index)
{
	return (pmd_t *)pud_page_vaddr(*pud) + index;
}

static void print_pmd_tree(pud_t *pud, uint64_t identifier)
{
	int i;

	for (i = 0; i < PTRS_PER_PMD; i++) {
		pmd_t *pmd = pmd_indexer(pud, i);
		unsigned long pfn;

		if (pmd_none(*pmd) || unlikely(pmd_bad(*pmd)))
			continue;

		pfn = pmd_pfn(*pmd);

		if (pfn)
			pr_warn("[HVM-%llu] \t\t\tPUD->PMD[%d] at pfn %lx\n", identifier, i, pfn);

		// this will print user page pfns, we only want page table pfns
		// print_pte_tree(pmd, identifier);
	}
}

static inline pud_t *pud_indexer(p4d_t *p4d, int index)
{
	return (pud_t *)p4d_page_vaddr(*p4d) + index;
}

static void print_pud_tree(p4d_t *p4d, uint64_t identifier)
{
	int i;

	for (i = 0; i < PTRS_PER_PUD; i++) {
		pud_t *pud = pud_indexer(p4d, i);
		unsigned long pfn;

		if (pud_none(*pud) || unlikely(pud_bad(*pud)))
			continue;

		pfn = pud_pfn(*pud);

		if (pfn)
			pr_warn("[HVM-%llu] \t\tP4D->PUD[%d] at pfn %lx\n", identifier, i, pfn);

		print_pmd_tree(pud, identifier);
	}
}

static inline p4d_t *p4d_indexer(pgd_t *pgd, int index)
{
	if (!pgtable_l5_enabled())
		return (p4d_t *)pgd;
	return (p4d_t *)pgd_page_vaddr(*pgd) + index;
}

static void print_p4d_tree(pgd_t *pgd, uint64_t identifier)
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
			pr_warn("[HVM-%llu] \tPGD->P4D[%d] at pfn %lx\n", identifier, i, pfn);

		print_pud_tree(p4d, identifier);
	}
}

static inline pgd_t *pgd_indexer(struct mm_struct *mm, int index)
{
	return (mm->pgd + index);
}

static void print_pgd_tree(struct mm_struct *mm, uint64_t identifier)
{
	int i;

	for (i = 0; i < PTRS_PER_PGD; i++) {
		pgd_t *pgd = pgd_indexer(mm, i);
		unsigned long pfn;

#ifndef CONFIG_HVM_DEBUG
		// don't print kernel layout info
		if (i >= PTRS_PER_PGD / 2)
			break;
#endif

		if (pgd_none(*pgd) || unlikely(pgd_bad(*pgd)))
			continue;

		pfn = pgd_pfn(*pgd);

		if (pfn)
			pr_warn("[HVM-%llu] ROOT->PGD[%d] at pfn %lx\n", identifier, i, pfn);

		print_p4d_tree(pgd, identifier);
	}
}

SYSCALL_DEFINE2(show_pgtbl, pid_t, task_id, uint64_t, identifier)
{
	struct mm_struct *mm;

	if (task_id == 0) task_id = current->pid;

	// as an experiment routine, we will simply trust the caller
	mm = get_mm_from_pid(task_id, false);
	if (!mm) return -EINVAL;

	print_pgd_tree(mm, identifier);

	return 1;
}

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
