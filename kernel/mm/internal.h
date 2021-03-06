
#ifndef __MM_INTERNAL_H
#define __MM_INTERNAL_H

#include <linux/mm.h>

void free_pgtables(struct mmu_gather *tlb, struct vm_area_struct *start_vma,
		unsigned long floor, unsigned long ceiling);

extern void prep_compound_page(struct page *page, unsigned long order);
extern void prep_compound_gigantic_page(struct page *page, unsigned long order);

static inline void set_page_count(struct page *page, int v)
{
	atomic_set(&page->_count, v);
}

static inline void set_page_refcounted(struct page *page)
{
	VM_BUG_ON(PageTail(page));
	VM_BUG_ON(atomic_read(&page->_count));
	set_page_count(page, 1);
}

static inline void __put_page(struct page *page)
{
	atomic_dec(&page->_count);
}

extern int isolate_lru_page(struct page *page);
extern void putback_lru_page(struct page *page);

extern unsigned long highest_memmap_pfn;
extern void __free_pages_bootmem(struct page *page, unsigned int order);

static inline unsigned long page_order(struct page *page)
{
	VM_BUG_ON(!PageBuddy(page));
	return page_private(page);
}

extern long mlock_vma_pages_range(struct vm_area_struct *vma,
			unsigned long start, unsigned long end);
extern void munlock_vma_pages_range(struct vm_area_struct *vma,
			unsigned long start, unsigned long end);
static inline void munlock_vma_pages_all(struct vm_area_struct *vma)
{
	munlock_vma_pages_range(vma, vma->vm_start, vma->vm_end);
}

#ifdef CONFIG_UNEVICTABLE_LRU
static inline void unevictable_migrate_page(struct page *new, struct page *old)
{
	if (TestClearPageUnevictable(old))
		SetPageUnevictable(new);
}
#else
static inline void unevictable_migrate_page(struct page *new, struct page *old)
{
}
#endif

#ifdef CONFIG_UNEVICTABLE_LRU
static inline int is_mlocked_vma(struct vm_area_struct *vma, struct page *page)
{
	VM_BUG_ON(PageLRU(page));

	if (likely((vma->vm_flags & (VM_LOCKED | VM_SPECIAL)) != VM_LOCKED))
		return 0;

	if (!TestSetPageMlocked(page)) {
		inc_zone_page_state(page, NR_MLOCK);
		count_vm_event(UNEVICTABLE_PGMLOCKED);
	}
	return 1;
}

extern void mlock_vma_page(struct page *page);

extern void __clear_page_mlock(struct page *page);
static inline void clear_page_mlock(struct page *page)
{
	if (unlikely(TestClearPageMlocked(page)))
		__clear_page_mlock(page);
}

static inline void mlock_migrate_page(struct page *newpage, struct page *page)
{
	if (TestClearPageMlocked(page)) {
		unsigned long flags;

		local_irq_save(flags);
		__dec_zone_page_state(page, NR_MLOCK);
		SetPageMlocked(newpage);
		__inc_zone_page_state(newpage, NR_MLOCK);
		local_irq_restore(flags);
	}
}

static inline void free_page_mlock(struct page *page)
{
	if (unlikely(TestClearPageMlocked(page))) {
		unsigned long flags;

		local_irq_save(flags);
		__dec_zone_page_state(page, NR_MLOCK);
		__count_vm_event(UNEVICTABLE_MLOCKFREED);
		local_irq_restore(flags);
	}
}

#else /* CONFIG_UNEVICTABLE_LRU */
static inline int is_mlocked_vma(struct vm_area_struct *v, struct page *p)
{
	return 0;
}
static inline void clear_page_mlock(struct page *page) { }
static inline void mlock_vma_page(struct page *page) { }
static inline void mlock_migrate_page(struct page *new, struct page *old) { }
static inline void free_page_mlock(struct page *page) { }

#endif /* CONFIG_UNEVICTABLE_LRU */

static inline struct page *mem_map_offset(struct page *base, int offset)
{
	if (unlikely(offset >= MAX_ORDER_NR_PAGES))
		return pfn_to_page(page_to_pfn(base) + offset);
	return base + offset;
}

static inline struct page *mem_map_next(struct page *iter,
						struct page *base, int offset)
{
	if (unlikely((offset & (MAX_ORDER_NR_PAGES - 1)) == 0)) {
		unsigned long pfn = page_to_pfn(base) + offset;
		if (!pfn_valid(pfn))
			return NULL;
		return pfn_to_page(pfn);
	}
	return iter + 1;
}

#ifdef CONFIG_SPARSEMEM
#define __paginginit __meminit
#else
#define __paginginit __init
#endif

/* Memory initialisation debug and verification */
enum mminit_level {
	MMINIT_WARNING,
	MMINIT_VERIFY,
	MMINIT_TRACE
};

#ifdef CONFIG_DEBUG_MEMORY_INIT

extern int mminit_loglevel;

#define mminit_dprintk(level, prefix, fmt, arg...) \
do { \
	if (level < mminit_loglevel) { \
		printk(level <= MMINIT_WARNING ? KERN_WARNING : KERN_DEBUG); \
		printk(KERN_CONT "mminit::" prefix " " fmt, ##arg); \
	} \
} while (0)

extern void mminit_verify_pageflags_layout(void);
extern void mminit_verify_page_links(struct page *page,
		enum zone_type zone, unsigned long nid, unsigned long pfn);
extern void mminit_verify_zonelist(void);

#else

static inline void mminit_dprintk(enum mminit_level level,
				const char *prefix, const char *fmt, ...)
{
}

static inline void mminit_verify_pageflags_layout(void)
{
}

static inline void mminit_verify_page_links(struct page *page,
		enum zone_type zone, unsigned long nid, unsigned long pfn)
{
}

static inline void mminit_verify_zonelist(void)
{
}
#endif /* CONFIG_DEBUG_MEMORY_INIT */

/* mminit_validate_memmodel_limits is independent of CONFIG_DEBUG_MEMORY_INIT */
#if defined(CONFIG_SPARSEMEM)
extern void mminit_validate_memmodel_limits(unsigned long *start_pfn,
				unsigned long *end_pfn);
#else
static inline void mminit_validate_memmodel_limits(unsigned long *start_pfn,
				unsigned long *end_pfn)
{
}
#endif /* CONFIG_SPARSEMEM */

#define GUP_FLAGS_WRITE                  0x1
#define GUP_FLAGS_FORCE                  0x2
#define GUP_FLAGS_IGNORE_VMA_PERMISSIONS 0x4
#define GUP_FLAGS_IGNORE_SIGKILL         0x8

int __get_user_pages(struct task_struct *tsk, struct mm_struct *mm,
		     unsigned long start, int len, int flags,
		     struct page **pages, struct vm_area_struct **vmas);

#endif
