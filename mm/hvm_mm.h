#ifndef __MM_HVM_MM_H
#define __MM_HVM_MM_H

#ifdef CONFIG_HVM_HOST

#define hvm_static

// hvm_debug.c

extern int madvise_hijack_enabled;
bool is_madvise_hijack_target(struct task_struct *task);

// huge_memory.c

int demote_memory_region(struct vm_area_struct *vma, unsigned long start, unsigned long end);

#else /* CONFIG_HVM_HOST */

#define hvm_static static

#endif /* CONFIG_HVM_HOST */

#endif /* __MM_HVM_MM_H */
