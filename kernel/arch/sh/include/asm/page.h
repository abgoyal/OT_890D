
#ifndef __ASM_SH_PAGE_H
#define __ASM_SH_PAGE_H


#include <linux/const.h>

/* PAGE_SHIFT determines the page size */
#if defined(CONFIG_PAGE_SIZE_4KB)
# define PAGE_SHIFT	12
#elif defined(CONFIG_PAGE_SIZE_8KB)
# define PAGE_SHIFT	13
#elif defined(CONFIG_PAGE_SIZE_16KB)
# define PAGE_SHIFT	14
#elif defined(CONFIG_PAGE_SIZE_64KB)
# define PAGE_SHIFT	16
#else
# error "Bogus kernel page size?"
#endif

#define PAGE_SIZE	(_AC(1, UL) << PAGE_SHIFT)
#define PAGE_MASK	(~(PAGE_SIZE-1))
#define PTE_MASK	PAGE_MASK

#if defined(CONFIG_HUGETLB_PAGE_SIZE_64K)
#define HPAGE_SHIFT	16
#elif defined(CONFIG_HUGETLB_PAGE_SIZE_256K)
#define HPAGE_SHIFT	18
#elif defined(CONFIG_HUGETLB_PAGE_SIZE_1MB)
#define HPAGE_SHIFT	20
#elif defined(CONFIG_HUGETLB_PAGE_SIZE_4MB)
#define HPAGE_SHIFT	22
#elif defined(CONFIG_HUGETLB_PAGE_SIZE_64MB)
#define HPAGE_SHIFT	26
#elif defined(CONFIG_HUGETLB_PAGE_SIZE_512MB)
#define HPAGE_SHIFT	29
#endif

#ifdef CONFIG_HUGETLB_PAGE
#define HPAGE_SIZE		(1UL << HPAGE_SHIFT)
#define HPAGE_MASK		(~(HPAGE_SIZE-1))
#define HUGETLB_PAGE_ORDER	(HPAGE_SHIFT-PAGE_SHIFT)
#endif

#ifndef __ASSEMBLY__

extern unsigned long shm_align_mask;
extern unsigned long max_low_pfn, min_low_pfn;
extern unsigned long memory_start, memory_end;

extern void clear_page(void *to);
extern void copy_page(void *to, void *from);

#if !defined(CONFIG_CACHE_OFF) && defined(CONFIG_MMU) && \
	(defined(CONFIG_CPU_SH5) || defined(CONFIG_CPU_SH4) || \
	 defined(CONFIG_SH7705_CACHE_32KB))
struct page;
struct vm_area_struct;
extern void clear_user_page(void *to, unsigned long address, struct page *page);
extern void copy_user_page(void *to, void *from, unsigned long address,
			   struct page *page);
#if defined(CONFIG_CPU_SH4)
extern void copy_user_highpage(struct page *to, struct page *from,
			       unsigned long vaddr, struct vm_area_struct *vma);
#define __HAVE_ARCH_COPY_USER_HIGHPAGE
#endif
#else
#define clear_user_page(page, vaddr, pg)	clear_page(page)
#define copy_user_page(to, from, vaddr, pg)	copy_page(to, from)
#endif

#ifdef CONFIG_X2TLB
typedef struct { unsigned long pte_low, pte_high; } pte_t;
typedef struct { unsigned long long pgprot; } pgprot_t;
typedef struct { unsigned long long pgd; } pgd_t;
#define pte_val(x) \
	((x).pte_low | ((unsigned long long)(x).pte_high << 32))
#define __pte(x) \
	({ pte_t __pte = {(x), ((unsigned long long)(x)) >> 32}; __pte; })
#elif defined(CONFIG_SUPERH32)
typedef struct { unsigned long pte_low; } pte_t;
typedef struct { unsigned long pgprot; } pgprot_t;
typedef struct { unsigned long pgd; } pgd_t;
#define pte_val(x)	((x).pte_low)
#define __pte(x)	((pte_t) { (x) } )
#else
typedef struct { unsigned long long pte_low; } pte_t;
typedef struct { unsigned long pgprot; } pgprot_t;
typedef struct { unsigned long pgd; } pgd_t;
#define pte_val(x)	((x).pte_low)
#define __pte(x)	((pte_t) { (x) } )
#endif

#define pgd_val(x)	((x).pgd)
#define pgprot_val(x)	((x).pgprot)

#define __pgd(x) ((pgd_t) { (x) } )
#define __pgprot(x)	((pgprot_t) { (x) } )

typedef struct page *pgtable_t;

#define pte_pgprot(x) __pgprot(pte_val(x) & PTE_FLAGS_MASK)

#endif /* !__ASSEMBLY__ */

#define __MEMORY_START		CONFIG_MEMORY_START
#define __MEMORY_SIZE		CONFIG_MEMORY_SIZE

#define PAGE_OFFSET		CONFIG_PAGE_OFFSET

#ifdef CONFIG_32BIT
#define __pa(x)	((unsigned long)(x)-PAGE_OFFSET+__MEMORY_START)
#define __va(x)	((void *)((unsigned long)(x)+PAGE_OFFSET-__MEMORY_START))
#else
#define __pa(x)	((unsigned long)(x)-PAGE_OFFSET)
#define __va(x)	((void *)((unsigned long)(x)+PAGE_OFFSET))
#endif

#define pfn_to_kaddr(pfn)	__va((pfn) << PAGE_SHIFT)
#define page_to_phys(page)	(page_to_pfn(page) << PAGE_SHIFT)

#define PFN_START		(__MEMORY_START >> PAGE_SHIFT)
#define ARCH_PFN_OFFSET		(PFN_START)
#define virt_to_page(kaddr)	pfn_to_page(__pa(kaddr) >> PAGE_SHIFT)
#ifdef CONFIG_FLATMEM
#define pfn_valid(pfn)		((pfn) >= min_low_pfn && (pfn) < max_low_pfn)
#endif
#define virt_addr_valid(kaddr)	pfn_valid(__pa(kaddr) >> PAGE_SHIFT)

#define VM_DATA_DEFAULT_FLAGS	(VM_READ | VM_WRITE | VM_EXEC | \
				 VM_MAYREAD | VM_MAYWRITE | VM_MAYEXEC)

#include <asm-generic/memory_model.h>
#include <asm-generic/page.h>

/* vDSO support */
#ifdef CONFIG_VSYSCALL
#define __HAVE_ARCH_GATE_AREA
#endif

#define ARCH_KMALLOC_MINALIGN	L1_CACHE_BYTES

#ifdef CONFIG_SUPERH64
#define ARCH_SLAB_MINALIGN	8
#endif

#endif /* __ASM_SH_PAGE_H */