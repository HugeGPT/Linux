#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/mm.h>
#include <linux/sched/mm.h>
#include <linux/mm_inline.h>
#include <linux/syscalls.h>
#include <linux/gfp.h>
#include <linux/hugetlb.h>
#include <linux/spinlock.h>
#include <linux/sched.h>
#include <linux/kernel.h>
#include <linux/kvm_para.h>
#include <linux/hivemind.h>


#include <asm/tlb.h>
#include <asm/pgalloc.h>
#include <asm/fixmap.h>
#include <asm/mtrr.h>

// global data and accessors

int hvm_pool_gfp[HVM_POOL_KINDS] = {
	[HVM_KERNEL]    = GFP_PGTABLE_KERNEL,
	[HVM_USER]      = GFP_PGTABLE_USER,
#ifdef CONFIG_HIGHPTE
	[HVM_USER_HIGH] = GFP_PGTABLE_USER | __GFP_HIGHMEM,
#endif
};

#ifdef CONFIG_HVM_GUEST

#define hvm_pool(kind) (&hvm_page_pools[kind])

#define hvm_lock_pool(ptr) spin_lock(&ptr->spinlock)
#define hvm_unlock_pool(ptr) spin_unlock(&ptr->spinlock)

#define hvm_pool_group(ptr) (&ptr->group)
#define hvm_task_group(ptr) (&ptr->hvm_task_cache)
#define hvm_group_used(ptr) (ptr->head + ptr->tail)

static bool hvm_init_done = false;
static struct hvm_page_pool hvm_page_pools[HVM_POOL_KINDS];

// @lock_context: irrelevant
// @lock_state: x -> x
bool hivemind_init_done(void)
{
	return hvm_init_done;
}

// group operations

// @lock_context: irrelevant
// @lock_state: x -> x
static struct page *hivemind_group_alloc_page_nostd_impl(struct hvm_page_group *group, int order)
{
	int page_sz = 1 << order;
	int natural_tail_inv = group->total - group->tail - page_sz;
	int natural_tail = group->total - natural_tail_inv;
	int aligned_tail_inv = ALIGN_DOWN(natural_tail_inv, 1 << order);
	int aligned_tail = group->total - aligned_tail_inv;
	int i, delta;
	struct page *alloc;

	// |< page
	// |--------- total -------->|
	//                   |< tail |
	//             |< sz |
	// | natl inv >|< natl tail  |
	//        |< d |
	// | ainv>|<  aligned tail   |
	//              {free}
	//        { ret }
	//        {     new tail     }

	// free space check
	if (aligned_tail_inv < group->head)
		return NULL;

	alloc = group->page + aligned_tail_inv;
	group->tail = aligned_tail;

	// release free space (d)
	delta = aligned_tail - natural_tail;
	for (i = 0; i < delta; i++) {
		__free_page(alloc + page_sz + i);
	}

	return alloc;
}

// @lock_context: irrelevant
// @lock_state: x -> x
static struct page *hivemind_group_alloc_page_impl(struct hvm_page_group *group, int order, bool standard)
{
	int page_sz = 1 << order;
	struct page *alloc;

	// |< page
	// | head >|
	//         | sz >|
	//         { ret }
	// {   new head  }

	if (hvm_group_used(group) + page_sz > group->total)
		return NULL;
	
	if (likely(standard)) {
		alloc = group->page + group->head;
		group->head += page_sz;

		return alloc;
	}

	return hivemind_group_alloc_page_nostd_impl(group, order);
}

// @lock_context: irrelevant
// @lock_state: x -> x
static void hivemind_group_free_page_impl(struct hvm_page_group *group)
{
	int i;
	int unused = group->total - hvm_group_used(group);
	struct page *unused_pg;

	BUG_ON(unused < 0);

	if (group->page == NULL)
		goto mark_freed;

	unused_pg = group->page + group->head;

	for (i = 0; i < unused; i++) {
		__free_page(unused_pg + i);
	}

mark_freed:
	group->page = NULL;
	group->head = 0;
	group->tail = 0;
	group->total = 0;

	return;
}

// paravirt

// @lock_context: require unlocked
// @lock_state: x -> x
static int hivemind_paravirt_kvm_req_hugepage(struct page *head, int count)
{
	int i;

	if (hvm_demote_page_enabled) {
		request_demote(page_to_pfn(head), count);
	}

	return 0;
}

// pool charger

// @lock_context: require unlocked
// @lock_state: x -> x
static struct page *hivemind_pool_reserve_page_impl(struct hvm_page_pool *pool)
{
	struct page *new_pool_page;

	new_pool_page = alloc_pages(pool->gfp, HVM_HUGE_PAGE_ORDER);
	if (!new_pool_page)
		return NULL;

	hivemind_paravirt_kvm_req_hugepage(new_pool_page, 1);

	return new_pool_page;
}

// @lock_context: require unlocked
// @lock_state: x -> x
static void hivemind_pool_release_page_impl(struct page *page)
{
	if (page)
		__free_pages(page, HVM_HUGE_PAGE_ORDER);
}

// @lock_context: irrelevant
// @lock_state: x -> x
static void hivemind_pool_charge_page_impl(struct hvm_page_pool *pool, struct page *page)
{
	struct hvm_page_group *pool_grp = hvm_pool_group(pool);

#ifdef CONFIG_HVM_DEBUG_VERBOSE
	pr_warn("hivemind_pool_charge_page_impl - charged %lx\n", page_to_pfn(page));
#endif
	hivemind_group_free_page_impl(pool_grp);
	pool_grp->page = page;
	pool_grp->total = HVM_HUGE_PAGE_TOTAL;
}

// pool operations

// @lock_context: require unlocked
// @lock_state: x -> x
static struct page *hivemind_pool_alloc_page(struct hvm_page_pool *pool, int order, bool standard)
{
	struct hvm_page_group *pool_grp = hvm_pool_group(pool);
	struct page *alloc;
	struct page *charger = NULL;

	BUG_ON(standard && order != HVM_PAGE_GROUP_ORDER);
	// On x64 there seems like no PTP will be bigger than order 1
	// With CONFIG_PAGE_TABLE_ISOLATION off, all PTPs are order 0 (PGD_ALLOCATION_ORDER)
	// This is to check if anything is missing
	BUG_ON(!standard && order > PGD_ALLOCATION_ORDER);

	hvm_lock_pool(pool);

	// first try
	alloc = hivemind_group_alloc_page_impl(pool_grp, order, standard);
	if (alloc)
		goto success;
	
	hvm_unlock_pool(pool);

	// cannot alloc, prepare a charger page
	charger = hivemind_pool_reserve_page_impl(pool);

	hvm_lock_pool(pool);

	// is someone charged the pool when we were outside the lock?
	alloc = hivemind_group_alloc_page_impl(pool_grp, order, standard);
	if (alloc)
		goto success;

	// charge page not allocated
	if (!charger) {
		pr_warn("hivemind_pool_alloc_page - charge NOMEM\n");
		goto nomem;
	}

	// recharge
	hivemind_pool_charge_page_impl(pool, charger);
	charger = NULL;

	// try again
	alloc = hivemind_group_alloc_page_impl(pool_grp, order, standard);
	if (alloc)
		goto success;

nomem:
	hvm_unlock_pool(pool);
	pr_warn("hivemind_pool_alloc_page - alloc NOMEM\n");
	return NULL;

success:
	// charger not used, release it
	if (charger)
		hivemind_pool_release_page_impl(charger);

	hvm_unlock_pool(pool);
	return alloc;
}

// @lock_context: require unlocked
// @lock_state: x -> x
static int hivemind_pool_alloc_group(struct hvm_page_pool *pool, struct hvm_page_group *group)
{
	struct page *alloc;
	
	alloc = hivemind_pool_alloc_page(pool, HVM_PAGE_GROUP_ORDER, /* standard */ true);
	if (alloc == NULL)
		return -ENOMEM;

	hivemind_group_free_page_impl(group);
	group->page = alloc;
	group->total = HVM_PAGE_GROUP_SIZE;

	return 0;
}

// task local group allocator

// @lock_context: require unlocked
// @lock_state: x -> x
static struct page *hivemind_task_alloc_pages(struct task_struct *task, struct hvm_page_pool *pool, int order)
{
	struct hvm_page_group *tsk_group = hvm_task_group(task);
	struct page *alloc = NULL;
	int ret = 0;

	// don't need lock here, task local pool ops are single-threaded
	alloc = hivemind_group_alloc_page_impl(tsk_group, order, /* standard */ order == 0);
	if (alloc)
		return alloc;
	
	ret = hivemind_pool_alloc_group(pool, tsk_group);
	if (ret) {
		pr_warn("hivemind_alloc - error when refilling group: %d\n", ret);
		return NULL;
	}
	
	alloc = hivemind_group_alloc_page_impl(tsk_group, order, /* standard */ order == 0);
	return alloc;
}

// @lock_context: require unlocked
// @lock_state: x -> x
static struct page *hivemind_kind_alloc_pages(struct task_struct *task, int kind, int order)
{
	struct page *alloc = NULL;

	if (task)
		alloc = hivemind_task_alloc_pages(task, hvm_pool(kind), order);
	
	if (alloc)
		return alloc;

	// no task or task pool failed, direct pull from pool
	return hivemind_pool_alloc_page(hvm_pool(kind), order, /* standard */ HVM_PAGE_GROUP_ORDER == order);
}

// public interface

// @lock_context: require unlocked
// @lock_state: x -> x
struct page *hivemind_alloc_pages(struct mm_struct *mm, gfp_t gfp, int order)
{
	int kind;
	bool found = false;
	struct task_struct *task = NULL;
	struct page *alloc = NULL;

	if (!hivemind_init_done())
		return alloc_pages(gfp, order);

	if (mm && mm != &init_mm) {
		// task alloc has some problems, see devnote.txt for detail
		// task = READ_ONCE(mm->owner);
	}

	for (kind = HVM_KERNEL; kind < HVM_POOL_KINDS; kind++) {
		if (hvm_pool_gfp[kind] == gfp) {
			found = true;
			break;
		}
	}

	if (found)
		alloc = hivemind_kind_alloc_pages(task, kind, order);
	
	if (alloc)
		return alloc;
	
	pr_warn("hivemind_alloc_pages - failed with gfp %x\n", gfp);
	return alloc_pages(gfp, order);
}

// @lock_context: require unlocked
// @lock_state: x -> x
void hivemind_notify_setting_event(void) {
	int kind;

	for (kind = HVM_KERNEL; kind < HVM_POOL_KINDS; kind++) {
		struct hvm_page_pool *pool = hvm_pool(kind);
		struct hvm_page_group *pool_grp = hvm_pool_group(pool);

		hvm_lock_pool(pool);
		if (pool_grp->page)
			request_demote(page_to_pfn(pool_grp->page), HVM_HUGE_PAGE_TOTAL);
		hvm_unlock_pool(pool);
	}
}

// initializer

// @lock_context: require exclusive
// @lock_state: x -> x
static void hivemind_create_page_pools(void)
{
	int kind;

	for (kind = HVM_KERNEL; kind < HVM_POOL_KINDS; kind++) {
		struct hvm_page_pool *hvm_pool;

		hvm_page_pools[kind] = (struct hvm_page_pool) { 0 };

		hvm_pool = hvm_pool(kind);

		hvm_pool->gfp = hvm_pool_gfp[kind];
		spin_lock_init(&hvm_pool->spinlock);
	}
}

// @lock_context: require unlocked, require exclusive
// @lock_state: x -> x
int hivemind_init(void)
{
	int ret = 0;
	int kind;

	if	(hivemind_init_done())
		return 0;

	hivemind_create_page_pools();

	for (kind = HVM_KERNEL; kind < HVM_POOL_KINDS; kind++) {
		struct hvm_page_pool *pool = hvm_pool(kind);
		struct page *page = NULL;

		page = hivemind_pool_reserve_page_impl(pool);
		if (!page) {
			pr_warn("hivemind_init - error when refilling pool: NOMEM\n");
			break;
		}

		hivemind_pool_charge_page_impl(pool, page);
	}

	if (!ret)
		goto success;
	
	for (kind = HVM_KERNEL; kind < HVM_POOL_KINDS; kind++) {
		struct hvm_page_pool *pool = hvm_pool(kind);
		struct hvm_page_group *pool_grp = hvm_pool_group(pool);

		if (pool_grp->page)
			hivemind_pool_release_page_impl(pool_grp->page);

		pool_grp->page = NULL;
		pool_grp->head = 0;
		pool_grp->tail = 0;
		pool_grp->total = 0;
	}

success:
	hvm_init_done = ret == 0;
	pr_warn("hivemind_init - init done: %d\n", hvm_init_done);
	return ret;
}

#else /* CONFIG_HVM_GUEST */

int hivemind_init(void) {
	return 0;
}

bool hivemind_init_done(void) {
	return false;
}

struct page *hivemind_alloc_pages(struct mm_struct *mm, gfp_t gfp, int order) {
	return alloc_pages(gfp, order);
}

void hivemind_notify_setting_event(void) {
	// nothing
}

#endif /* CONFIG_HVM_GUEST */
