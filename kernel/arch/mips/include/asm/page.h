
#ifndef _ASM_PAGE_H
#define _ASM_PAGE_H

#include <spaces.h>

#ifdef CONFIG_PAGE_SIZE_4KB
#define PAGE_SHIFT	12
#endif
#ifdef CONFIG_PAGE_SIZE_8KB
#define PAGE_SHIFT	13
#endif
#ifdef CONFIG_PAGE_SIZE_16KB
#define PAGE_SHIFT	14
#endif
#ifdef CONFIG_PAGE_SIZE_64KB
#define PAGE_SHIFT	16
#endif
#define PAGE_SIZE	(1UL << PAGE_SHIFT)
#define PAGE_MASK       (~((1 << PAGE_SHIFT) - 1))

#ifndef __ASSEMBLY__

#include <linux/pfn.h>
#include <asm/io.h>

extern void build_clear_page(void);
extern void build_copy_page(void);

#define ARCH_PFN_OFFSET		PFN_UP(PHYS_OFFSET)

extern void clear_page(void * page);
extern void copy_page(void * to, void * from);

extern unsigned long shm_align_mask;

static inline unsigned long pages_do_alias(unsigned long addr1,
	unsigned long addr2)
{
	return (addr1 ^ addr2) & shm_align_mask;
}

struct page;

static inline void clear_user_page(void *addr, unsigned long vaddr,
	struct page *page)
{
	extern void (*flush_data_cache_page)(unsigned long addr);

	clear_page(addr);
	if (pages_do_alias((unsigned long) addr, vaddr & PAGE_MASK))
		flush_data_cache_page((unsigned long)addr);
}

extern void copy_user_page(void *vto, void *vfrom, unsigned long vaddr,
	struct page *to);
struct vm_area_struct;
extern void copy_user_highpage(struct page *to, struct page *from,
	unsigned long vaddr, struct vm_area_struct *vma);

#define __HAVE_ARCH_COPY_USER_HIGHPAGE

#ifdef CONFIG_64BIT_PHYS_ADDR
  #ifdef CONFIG_CPU_MIPS32
    typedef struct { unsigned long pte_low, pte_high; } pte_t;
    #define pte_val(x)    ((x).pte_low | ((unsigned long long)(x).pte_high << 32))
    #define __pte(x)      ({ pte_t __pte = {(x), ((unsigned long long)(x)) >> 32}; __pte; })
  #else
     typedef struct { unsigned long long pte; } pte_t;
     #define pte_val(x)	((x).pte)
     #define __pte(x)	((pte_t) { (x) } )
  #endif
#else
typedef struct { unsigned long pte; } pte_t;
#define pte_val(x)	((x).pte)
#define __pte(x)	((pte_t) { (x) } )
#endif
typedef struct page *pgtable_t;

#ifdef CONFIG_64BIT

typedef struct { unsigned long pmd; } pmd_t;
#define pmd_val(x)	((x).pmd)
#define __pmd(x)	((pmd_t) { (x) } )

#endif


typedef struct { unsigned long pgd; } pgd_t;
#define pgd_val(x)	((x).pgd)
#define __pgd(x)	((pgd_t) { (x) } )

typedef struct { unsigned long pgprot; } pgprot_t;
#define pgprot_val(x)	((x).pgprot)
#define __pgprot(x)	((pgprot_t) { (x) } )

#define ptep_buddy(x)	((pte_t *)((unsigned long)(x) ^ sizeof(pte_t)))

#endif /* !__ASSEMBLY__ */

#ifdef CONFIG_64BIT
#define __pa(x)								\
({									\
    unsigned long __x = (unsigned long)(x);				\
    __x < CKSEG0 ? XPHYSADDR(__x) : CPHYSADDR(__x);			\
})
#else
#define __pa(x)								\
    ((unsigned long)(x) - PAGE_OFFSET + PHYS_OFFSET)
#endif
#define __va(x)		((void *)((unsigned long)(x) + PAGE_OFFSET - PHYS_OFFSET))
#define __pa_symbol(x)	__pa(RELOC_HIDE((unsigned long)(x), 0))

#define pfn_to_kaddr(pfn)	__va((pfn) << PAGE_SHIFT)

#ifdef CONFIG_FLATMEM

#define pfn_valid(pfn)		((pfn) >= ARCH_PFN_OFFSET && (pfn) < max_mapnr)

#elif defined(CONFIG_SPARSEMEM)

/* pfn_valid is defined in linux/mmzone.h */

#elif defined(CONFIG_NEED_MULTIPLE_NODES)

#define pfn_valid(pfn)							\
({									\
	unsigned long __pfn = (pfn);					\
	int __n = pfn_to_nid(__pfn);					\
	((__n >= 0) ? (__pfn < NODE_DATA(__n)->node_start_pfn +		\
	                       NODE_DATA(__n)->node_spanned_pages)	\
	            : 0);						\
})

#endif

#define virt_to_page(kaddr)	pfn_to_page(PFN_DOWN(virt_to_phys(kaddr)))
#define virt_addr_valid(kaddr)	pfn_valid(PFN_DOWN(virt_to_phys(kaddr)))

#define VM_DATA_DEFAULT_FLAGS	(VM_READ | VM_WRITE | VM_EXEC | \
				 VM_MAYREAD | VM_MAYWRITE | VM_MAYEXEC)

#define UNCAC_ADDR(addr)	((addr) - PAGE_OFFSET + UNCAC_BASE)
#define CAC_ADDR(addr)		((addr) - UNCAC_BASE + PAGE_OFFSET)

#include <asm-generic/memory_model.h>
#include <asm-generic/page.h>

#endif /* _ASM_PAGE_H */
