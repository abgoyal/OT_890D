
#ifndef _LINUX_MMU_NOTIFIER_H
#define _LINUX_MMU_NOTIFIER_H

#include <linux/list.h>
#include <linux/spinlock.h>
#include <linux/mm_types.h>

struct mmu_notifier;
struct mmu_notifier_ops;

#ifdef CONFIG_MMU_NOTIFIER

struct mmu_notifier_mm {
	/* all mmu notifiers registerd in this mm are queued in this list */
	struct hlist_head list;
	/* to serialize the list modifications and hlist_unhashed */
	spinlock_t lock;
};

struct mmu_notifier_ops {
	/*
	 * Called either by mmu_notifier_unregister or when the mm is
	 * being destroyed by exit_mmap, always before all pages are
	 * freed. This can run concurrently with other mmu notifier
	 * methods (the ones invoked outside the mm context) and it
	 * should tear down all secondary mmu mappings and freeze the
	 * secondary mmu. If this method isn't implemented you've to
	 * be sure that nothing could possibly write to the pages
	 * through the secondary mmu by the time the last thread with
	 * tsk->mm == mm exits.
	 *
	 * As side note: the pages freed after ->release returns could
	 * be immediately reallocated by the gart at an alias physical
	 * address with a different cache model, so if ->release isn't
	 * implemented because all _software_ driven memory accesses
	 * through the secondary mmu are terminated by the time the
	 * last thread of this mm quits, you've also to be sure that
	 * speculative _hardware_ operations can't allocate dirty
	 * cachelines in the cpu that could not be snooped and made
	 * coherent with the other read and write operations happening
	 * through the gart alias address, so leading to memory
	 * corruption.
	 */
	void (*release)(struct mmu_notifier *mn,
			struct mm_struct *mm);

	/*
	 * clear_flush_young is called after the VM is
	 * test-and-clearing the young/accessed bitflag in the
	 * pte. This way the VM will provide proper aging to the
	 * accesses to the page through the secondary MMUs and not
	 * only to the ones through the Linux pte.
	 */
	int (*clear_flush_young)(struct mmu_notifier *mn,
				 struct mm_struct *mm,
				 unsigned long address);

	/*
	 * Before this is invoked any secondary MMU is still ok to
	 * read/write to the page previously pointed to by the Linux
	 * pte because the page hasn't been freed yet and it won't be
	 * freed until this returns. If required set_page_dirty has to
	 * be called internally to this method.
	 */
	void (*invalidate_page)(struct mmu_notifier *mn,
				struct mm_struct *mm,
				unsigned long address);

	/*
	 * invalidate_range_start() and invalidate_range_end() must be
	 * paired and are called only when the mmap_sem and/or the
	 * locks protecting the reverse maps are held. The subsystem
	 * must guarantee that no additional references are taken to
	 * the pages in the range established between the call to
	 * invalidate_range_start() and the matching call to
	 * invalidate_range_end().
	 *
	 * Invalidation of multiple concurrent ranges may be
	 * optionally permitted by the driver. Either way the
	 * establishment of sptes is forbidden in the range passed to
	 * invalidate_range_begin/end for the whole duration of the
	 * invalidate_range_begin/end critical section.
	 *
	 * invalidate_range_start() is called when all pages in the
	 * range are still mapped and have at least a refcount of one.
	 *
	 * invalidate_range_end() is called when all pages in the
	 * range have been unmapped and the pages have been freed by
	 * the VM.
	 *
	 * The VM will remove the page table entries and potentially
	 * the page between invalidate_range_start() and
	 * invalidate_range_end(). If the page must not be freed
	 * because of pending I/O or other circumstances then the
	 * invalidate_range_start() callback (or the initial mapping
	 * by the driver) must make sure that the refcount is kept
	 * elevated.
	 *
	 * If the driver increases the refcount when the pages are
	 * initially mapped into an address space then either
	 * invalidate_range_start() or invalidate_range_end() may
	 * decrease the refcount. If the refcount is decreased on
	 * invalidate_range_start() then the VM can free pages as page
	 * table entries are removed.  If the refcount is only
	 * droppped on invalidate_range_end() then the driver itself
	 * will drop the last refcount but it must take care to flush
	 * any secondary tlb before doing the final free on the
	 * page. Pages will no longer be referenced by the linux
	 * address space but may still be referenced by sptes until
	 * the last refcount is dropped.
	 */
	void (*invalidate_range_start)(struct mmu_notifier *mn,
				       struct mm_struct *mm,
				       unsigned long start, unsigned long end);
	void (*invalidate_range_end)(struct mmu_notifier *mn,
				     struct mm_struct *mm,
				     unsigned long start, unsigned long end);
};

struct mmu_notifier {
	struct hlist_node hlist;
	const struct mmu_notifier_ops *ops;
};

static inline int mm_has_notifiers(struct mm_struct *mm)
{
	return unlikely(mm->mmu_notifier_mm);
}

extern int mmu_notifier_register(struct mmu_notifier *mn,
				 struct mm_struct *mm);
extern int __mmu_notifier_register(struct mmu_notifier *mn,
				   struct mm_struct *mm);
extern void mmu_notifier_unregister(struct mmu_notifier *mn,
				    struct mm_struct *mm);
extern void __mmu_notifier_mm_destroy(struct mm_struct *mm);
extern void __mmu_notifier_release(struct mm_struct *mm);
extern int __mmu_notifier_clear_flush_young(struct mm_struct *mm,
					  unsigned long address);
extern void __mmu_notifier_invalidate_page(struct mm_struct *mm,
					  unsigned long address);
extern void __mmu_notifier_invalidate_range_start(struct mm_struct *mm,
				  unsigned long start, unsigned long end);
extern void __mmu_notifier_invalidate_range_end(struct mm_struct *mm,
				  unsigned long start, unsigned long end);

static inline void mmu_notifier_release(struct mm_struct *mm)
{
	if (mm_has_notifiers(mm))
		__mmu_notifier_release(mm);
}

static inline int mmu_notifier_clear_flush_young(struct mm_struct *mm,
					  unsigned long address)
{
	if (mm_has_notifiers(mm))
		return __mmu_notifier_clear_flush_young(mm, address);
	return 0;
}

static inline void mmu_notifier_invalidate_page(struct mm_struct *mm,
					  unsigned long address)
{
	if (mm_has_notifiers(mm))
		__mmu_notifier_invalidate_page(mm, address);
}

static inline void mmu_notifier_invalidate_range_start(struct mm_struct *mm,
				  unsigned long start, unsigned long end)
{
	if (mm_has_notifiers(mm))
		__mmu_notifier_invalidate_range_start(mm, start, end);
}

static inline void mmu_notifier_invalidate_range_end(struct mm_struct *mm,
				  unsigned long start, unsigned long end)
{
	if (mm_has_notifiers(mm))
		__mmu_notifier_invalidate_range_end(mm, start, end);
}

static inline void mmu_notifier_mm_init(struct mm_struct *mm)
{
	mm->mmu_notifier_mm = NULL;
}

static inline void mmu_notifier_mm_destroy(struct mm_struct *mm)
{
	if (mm_has_notifiers(mm))
		__mmu_notifier_mm_destroy(mm);
}

#define ptep_clear_flush_notify(__vma, __address, __ptep)		\
({									\
	pte_t __pte;							\
	struct vm_area_struct *___vma = __vma;				\
	unsigned long ___address = __address;				\
	__pte = ptep_clear_flush(___vma, ___address, __ptep);		\
	mmu_notifier_invalidate_page(___vma->vm_mm, ___address);	\
	__pte;								\
})

#define ptep_clear_flush_young_notify(__vma, __address, __ptep)		\
({									\
	int __young;							\
	struct vm_area_struct *___vma = __vma;				\
	unsigned long ___address = __address;				\
	__young = ptep_clear_flush_young(___vma, ___address, __ptep);	\
	__young |= mmu_notifier_clear_flush_young(___vma->vm_mm,	\
						  ___address);		\
	__young;							\
})

#else /* CONFIG_MMU_NOTIFIER */

static inline void mmu_notifier_release(struct mm_struct *mm)
{
}

static inline int mmu_notifier_clear_flush_young(struct mm_struct *mm,
					  unsigned long address)
{
	return 0;
}

static inline void mmu_notifier_invalidate_page(struct mm_struct *mm,
					  unsigned long address)
{
}

static inline void mmu_notifier_invalidate_range_start(struct mm_struct *mm,
				  unsigned long start, unsigned long end)
{
}

static inline void mmu_notifier_invalidate_range_end(struct mm_struct *mm,
				  unsigned long start, unsigned long end)
{
}

static inline void mmu_notifier_mm_init(struct mm_struct *mm)
{
}

static inline void mmu_notifier_mm_destroy(struct mm_struct *mm)
{
}

#define ptep_clear_flush_young_notify ptep_clear_flush_young
#define ptep_clear_flush_notify ptep_clear_flush

#endif /* CONFIG_MMU_NOTIFIER */

#endif /* _LINUX_MMU_NOTIFIER_H */
