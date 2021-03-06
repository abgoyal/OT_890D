

#include <linux/version.h>
#include <linux/module.h>
#include <linux/mm.h>
#include <linux/suspend.h>
#include <linux/delay.h>
#include <linux/bitops.h>
#include <linux/spinlock.h>
#include <linux/kernel.h>
#include <linux/pm.h>
#include <linux/device.h>
#include <linux/init.h>
#include <linux/bootmem.h>
#include <linux/syscalls.h>
#include <linux/console.h>
#include <linux/highmem.h>
#include <linux/list.h>

#include <asm/uaccess.h>
#include <asm/mmu_context.h>
#include <asm/pgtable.h>
#include <asm/tlbflush.h>
#include <asm/io.h>

#include "power.h"

static int swsusp_page_is_free(struct page *);
static void swsusp_set_page_forbidden(struct page *);
static void swsusp_unset_page_forbidden(struct page *);

struct pbe *restore_pblist;

/* Pointer to an auxiliary buffer (1 page) */
static void *buffer;


#define PG_ANY		0
#define PG_SAFE		1
#define PG_UNSAFE_CLEAR	1
#define PG_UNSAFE_KEEP	0

static unsigned int allocated_unsafe_pages;

static void *get_image_page(gfp_t gfp_mask, int safe_needed)
{
	void *res;

	res = (void *)get_zeroed_page(gfp_mask);
	if (safe_needed)
		while (res && swsusp_page_is_free(virt_to_page(res))) {
			/* The page is unsafe, mark it for swsusp_free() */
			swsusp_set_page_forbidden(virt_to_page(res));
			allocated_unsafe_pages++;
			res = (void *)get_zeroed_page(gfp_mask);
		}
	if (res) {
		swsusp_set_page_forbidden(virt_to_page(res));
		swsusp_set_page_free(virt_to_page(res));
	}
	return res;
}

unsigned long get_safe_page(gfp_t gfp_mask)
{
	return (unsigned long)get_image_page(gfp_mask, PG_SAFE);
}

static struct page *alloc_image_page(gfp_t gfp_mask)
{
	struct page *page;

	page = alloc_page(gfp_mask);
	if (page) {
		swsusp_set_page_forbidden(page);
		swsusp_set_page_free(page);
	}
	return page;
}


static inline void free_image_page(void *addr, int clear_nosave_free)
{
	struct page *page;

	BUG_ON(!virt_addr_valid(addr));

	page = virt_to_page(addr);

	swsusp_unset_page_forbidden(page);
	if (clear_nosave_free)
		swsusp_unset_page_free(page);

	__free_page(page);
}

/* struct linked_page is used to build chains of pages */

#define LINKED_PAGE_DATA_SIZE	(PAGE_SIZE - sizeof(void *))

struct linked_page {
	struct linked_page *next;
	char data[LINKED_PAGE_DATA_SIZE];
} __attribute__((packed));

static inline void
free_list_of_pages(struct linked_page *list, int clear_page_nosave)
{
	while (list) {
		struct linked_page *lp = list->next;

		free_image_page(list, clear_page_nosave);
		list = lp;
	}
}


struct chain_allocator {
	struct linked_page *chain;	/* the chain */
	unsigned int used_space;	/* total size of objects allocated out
					 * of the current page
					 */
	gfp_t gfp_mask;		/* mask for allocating pages */
	int safe_needed;	/* if set, only "safe" pages are allocated */
};

static void
chain_init(struct chain_allocator *ca, gfp_t gfp_mask, int safe_needed)
{
	ca->chain = NULL;
	ca->used_space = LINKED_PAGE_DATA_SIZE;
	ca->gfp_mask = gfp_mask;
	ca->safe_needed = safe_needed;
}

static void *chain_alloc(struct chain_allocator *ca, unsigned int size)
{
	void *ret;

	if (LINKED_PAGE_DATA_SIZE - ca->used_space < size) {
		struct linked_page *lp;

		lp = get_image_page(ca->gfp_mask, ca->safe_needed);
		if (!lp)
			return NULL;

		lp->next = ca->chain;
		ca->chain = lp;
		ca->used_space = 0;
	}
	ret = ca->chain->data + ca->used_space;
	ca->used_space += size;
	return ret;
}


#define BM_END_OF_MAP	(~0UL)

#define BM_BITS_PER_BLOCK	(PAGE_SIZE << 3)

struct bm_block {
	struct list_head hook;	/* hook into a list of bitmap blocks */
	unsigned long start_pfn;	/* pfn represented by the first bit */
	unsigned long end_pfn;	/* pfn represented by the last bit plus 1 */
	unsigned long *data;	/* bitmap representing pages */
};

static inline unsigned long bm_block_bits(struct bm_block *bb)
{
	return bb->end_pfn - bb->start_pfn;
}

/* strcut bm_position is used for browsing memory bitmaps */

struct bm_position {
	struct bm_block *block;
	int bit;
};

struct memory_bitmap {
	struct list_head blocks;	/* list of bitmap blocks */
	struct linked_page *p_list;	/* list of pages used to store zone
					 * bitmap objects and bitmap block
					 * objects
					 */
	struct bm_position cur;	/* most recently used bit position */
};

/* Functions that operate on memory bitmaps */

static void memory_bm_position_reset(struct memory_bitmap *bm)
{
	bm->cur.block = list_entry(bm->blocks.next, struct bm_block, hook);
	bm->cur.bit = 0;
}

static void memory_bm_free(struct memory_bitmap *bm, int clear_nosave_free);

static int create_bm_block_list(unsigned long pages,
				struct list_head *list,
				struct chain_allocator *ca)
{
	unsigned int nr_blocks = DIV_ROUND_UP(pages, BM_BITS_PER_BLOCK);

	while (nr_blocks-- > 0) {
		struct bm_block *bb;

		bb = chain_alloc(ca, sizeof(struct bm_block));
		if (!bb)
			return -ENOMEM;
		list_add(&bb->hook, list);
	}

	return 0;
}

struct mem_extent {
	struct list_head hook;
	unsigned long start;
	unsigned long end;
};

static void free_mem_extents(struct list_head *list)
{
	struct mem_extent *ext, *aux;

	list_for_each_entry_safe(ext, aux, list, hook) {
		list_del(&ext->hook);
		kfree(ext);
	}
}

static int create_mem_extents(struct list_head *list, gfp_t gfp_mask)
{
	struct zone *zone;

	INIT_LIST_HEAD(list);

	for_each_zone(zone) {
		unsigned long zone_start, zone_end;
		struct mem_extent *ext, *cur, *aux;

		if (!populated_zone(zone))
			continue;

		zone_start = zone->zone_start_pfn;
		zone_end = zone->zone_start_pfn + zone->spanned_pages;

		list_for_each_entry(ext, list, hook)
			if (zone_start <= ext->end)
				break;

		if (&ext->hook == list || zone_end < ext->start) {
			/* New extent is necessary */
			struct mem_extent *new_ext;

			new_ext = kzalloc(sizeof(struct mem_extent), gfp_mask);
			if (!new_ext) {
				free_mem_extents(list);
				return -ENOMEM;
			}
			new_ext->start = zone_start;
			new_ext->end = zone_end;
			list_add_tail(&new_ext->hook, &ext->hook);
			continue;
		}

		/* Merge this zone's range of PFNs with the existing one */
		if (zone_start < ext->start)
			ext->start = zone_start;
		if (zone_end > ext->end)
			ext->end = zone_end;

		/* More merging may be possible */
		cur = ext;
		list_for_each_entry_safe_continue(cur, aux, list, hook) {
			if (zone_end < cur->start)
				break;
			if (zone_end < cur->end)
				ext->end = cur->end;
			list_del(&cur->hook);
			kfree(cur);
		}
	}

	return 0;
}

static int
memory_bm_create(struct memory_bitmap *bm, gfp_t gfp_mask, int safe_needed)
{
	struct chain_allocator ca;
	struct list_head mem_extents;
	struct mem_extent *ext;
	int error;

	chain_init(&ca, gfp_mask, safe_needed);
	INIT_LIST_HEAD(&bm->blocks);

	error = create_mem_extents(&mem_extents, gfp_mask);
	if (error)
		return error;

	list_for_each_entry(ext, &mem_extents, hook) {
		struct bm_block *bb;
		unsigned long pfn = ext->start;
		unsigned long pages = ext->end - ext->start;

		bb = list_entry(bm->blocks.prev, struct bm_block, hook);

		error = create_bm_block_list(pages, bm->blocks.prev, &ca);
		if (error)
			goto Error;

		list_for_each_entry_continue(bb, &bm->blocks, hook) {
			bb->data = get_image_page(gfp_mask, safe_needed);
			if (!bb->data) {
				error = -ENOMEM;
				goto Error;
			}

			bb->start_pfn = pfn;
			if (pages >= BM_BITS_PER_BLOCK) {
				pfn += BM_BITS_PER_BLOCK;
				pages -= BM_BITS_PER_BLOCK;
			} else {
				/* This is executed only once in the loop */
				pfn += pages;
			}
			bb->end_pfn = pfn;
		}
	}

	bm->p_list = ca.chain;
	memory_bm_position_reset(bm);
 Exit:
	free_mem_extents(&mem_extents);
	return error;

 Error:
	bm->p_list = ca.chain;
	memory_bm_free(bm, PG_UNSAFE_CLEAR);
	goto Exit;
}

static void memory_bm_free(struct memory_bitmap *bm, int clear_nosave_free)
{
	struct bm_block *bb;

	list_for_each_entry(bb, &bm->blocks, hook)
		if (bb->data)
			free_image_page(bb->data, clear_nosave_free);

	free_list_of_pages(bm->p_list, clear_nosave_free);

	INIT_LIST_HEAD(&bm->blocks);
}

static int memory_bm_find_bit(struct memory_bitmap *bm, unsigned long pfn,
				void **addr, unsigned int *bit_nr)
{
	struct bm_block *bb;

	/*
	 * Check if the pfn corresponds to the current bitmap block and find
	 * the block where it fits if this is not the case.
	 */
	bb = bm->cur.block;
	if (pfn < bb->start_pfn)
		list_for_each_entry_continue_reverse(bb, &bm->blocks, hook)
			if (pfn >= bb->start_pfn)
				break;

	if (pfn >= bb->end_pfn)
		list_for_each_entry_continue(bb, &bm->blocks, hook)
			if (pfn >= bb->start_pfn && pfn < bb->end_pfn)
				break;

	if (&bb->hook == &bm->blocks)
		return -EFAULT;

	/* The block has been found */
	bm->cur.block = bb;
	pfn -= bb->start_pfn;
	bm->cur.bit = pfn + 1;
	*bit_nr = pfn;
	*addr = bb->data;
	return 0;
}

static void memory_bm_set_bit(struct memory_bitmap *bm, unsigned long pfn)
{
	void *addr;
	unsigned int bit;
	int error;

	error = memory_bm_find_bit(bm, pfn, &addr, &bit);
	BUG_ON(error);
	set_bit(bit, addr);
}

static int mem_bm_set_bit_check(struct memory_bitmap *bm, unsigned long pfn)
{
	void *addr;
	unsigned int bit;
	int error;

	error = memory_bm_find_bit(bm, pfn, &addr, &bit);
	if (!error)
		set_bit(bit, addr);
	return error;
}

static void memory_bm_clear_bit(struct memory_bitmap *bm, unsigned long pfn)
{
	void *addr;
	unsigned int bit;
	int error;

	error = memory_bm_find_bit(bm, pfn, &addr, &bit);
	BUG_ON(error);
	clear_bit(bit, addr);
}

static int memory_bm_test_bit(struct memory_bitmap *bm, unsigned long pfn)
{
	void *addr;
	unsigned int bit;
	int error;

	error = memory_bm_find_bit(bm, pfn, &addr, &bit);
	BUG_ON(error);
	return test_bit(bit, addr);
}

static bool memory_bm_pfn_present(struct memory_bitmap *bm, unsigned long pfn)
{
	void *addr;
	unsigned int bit;

	return !memory_bm_find_bit(bm, pfn, &addr, &bit);
}


static unsigned long memory_bm_next_pfn(struct memory_bitmap *bm)
{
	struct bm_block *bb;
	int bit;

	bb = bm->cur.block;
	do {
		bit = bm->cur.bit;
		bit = find_next_bit(bb->data, bm_block_bits(bb), bit);
		if (bit < bm_block_bits(bb))
			goto Return_pfn;

		bb = list_entry(bb->hook.next, struct bm_block, hook);
		bm->cur.block = bb;
		bm->cur.bit = 0;
	} while (&bb->hook != &bm->blocks);

	memory_bm_position_reset(bm);
	return BM_END_OF_MAP;

 Return_pfn:
	bm->cur.bit = bit + 1;
	return bb->start_pfn + bit;
}


struct nosave_region {
	struct list_head list;
	unsigned long start_pfn;
	unsigned long end_pfn;
};

static LIST_HEAD(nosave_regions);


void __init
__register_nosave_region(unsigned long start_pfn, unsigned long end_pfn,
			 int use_kmalloc)
{
	struct nosave_region *region;

	if (start_pfn >= end_pfn)
		return;

	if (!list_empty(&nosave_regions)) {
		/* Try to extend the previous region (they should be sorted) */
		region = list_entry(nosave_regions.prev,
					struct nosave_region, list);
		if (region->end_pfn == start_pfn) {
			region->end_pfn = end_pfn;
			goto Report;
		}
	}
	if (use_kmalloc) {
		/* during init, this shouldn't fail */
		region = kmalloc(sizeof(struct nosave_region), GFP_KERNEL);
		BUG_ON(!region);
	} else
		/* This allocation cannot fail */
		region = alloc_bootmem_low(sizeof(struct nosave_region));
	region->start_pfn = start_pfn;
	region->end_pfn = end_pfn;
	list_add_tail(&region->list, &nosave_regions);
 Report:
	printk(KERN_INFO "PM: Registered nosave memory: %016lx - %016lx\n",
		start_pfn << PAGE_SHIFT, end_pfn << PAGE_SHIFT);
}

static struct memory_bitmap *forbidden_pages_map;

/* Set bits in this map correspond to free page frames. */
static struct memory_bitmap *free_pages_map;


void swsusp_set_page_free(struct page *page)
{
	if (free_pages_map)
		memory_bm_set_bit(free_pages_map, page_to_pfn(page));
}

static int swsusp_page_is_free(struct page *page)
{
	return free_pages_map ?
		memory_bm_test_bit(free_pages_map, page_to_pfn(page)) : 0;
}

void swsusp_unset_page_free(struct page *page)
{
	if (free_pages_map)
		memory_bm_clear_bit(free_pages_map, page_to_pfn(page));
}

static void swsusp_set_page_forbidden(struct page *page)
{
	if (forbidden_pages_map)
		memory_bm_set_bit(forbidden_pages_map, page_to_pfn(page));
}

int swsusp_page_is_forbidden(struct page *page)
{
	return forbidden_pages_map ?
		memory_bm_test_bit(forbidden_pages_map, page_to_pfn(page)) : 0;
}

static void swsusp_unset_page_forbidden(struct page *page)
{
	if (forbidden_pages_map)
		memory_bm_clear_bit(forbidden_pages_map, page_to_pfn(page));
}


static void mark_nosave_pages(struct memory_bitmap *bm)
{
	struct nosave_region *region;

	if (list_empty(&nosave_regions))
		return;

	list_for_each_entry(region, &nosave_regions, list) {
		unsigned long pfn;

		pr_debug("PM: Marking nosave pages: %016lx - %016lx\n",
				region->start_pfn << PAGE_SHIFT,
				region->end_pfn << PAGE_SHIFT);

		for (pfn = region->start_pfn; pfn < region->end_pfn; pfn++)
			if (pfn_valid(pfn)) {
				/*
				 * It is safe to ignore the result of
				 * mem_bm_set_bit_check() here, since we won't
				 * touch the PFNs for which the error is
				 * returned anyway.
				 */
				mem_bm_set_bit_check(bm, pfn);
			}
	}
}


int create_basic_memory_bitmaps(void)
{
	struct memory_bitmap *bm1, *bm2;
	int error = 0;

	BUG_ON(forbidden_pages_map || free_pages_map);

	bm1 = kzalloc(sizeof(struct memory_bitmap), GFP_KERNEL);
	if (!bm1)
		return -ENOMEM;

	error = memory_bm_create(bm1, GFP_KERNEL, PG_ANY);
	if (error)
		goto Free_first_object;

	bm2 = kzalloc(sizeof(struct memory_bitmap), GFP_KERNEL);
	if (!bm2)
		goto Free_first_bitmap;

	error = memory_bm_create(bm2, GFP_KERNEL, PG_ANY);
	if (error)
		goto Free_second_object;

	forbidden_pages_map = bm1;
	free_pages_map = bm2;
	mark_nosave_pages(forbidden_pages_map);

	pr_debug("PM: Basic memory bitmaps created\n");

	return 0;

 Free_second_object:
	kfree(bm2);
 Free_first_bitmap:
 	memory_bm_free(bm1, PG_UNSAFE_CLEAR);
 Free_first_object:
	kfree(bm1);
	return -ENOMEM;
}


void free_basic_memory_bitmaps(void)
{
	struct memory_bitmap *bm1, *bm2;

	BUG_ON(!(forbidden_pages_map && free_pages_map));

	bm1 = forbidden_pages_map;
	bm2 = free_pages_map;
	forbidden_pages_map = NULL;
	free_pages_map = NULL;
	memory_bm_free(bm1, PG_UNSAFE_CLEAR);
	kfree(bm1);
	memory_bm_free(bm2, PG_UNSAFE_CLEAR);
	kfree(bm2);

	pr_debug("PM: Basic memory bitmaps freed\n");
}


unsigned int snapshot_additional_pages(struct zone *zone)
{
	unsigned int res;

	res = DIV_ROUND_UP(zone->spanned_pages, BM_BITS_PER_BLOCK);
	res += DIV_ROUND_UP(res * sizeof(struct bm_block), PAGE_SIZE);
	return 2 * res;
}

#ifdef CONFIG_HIGHMEM

static unsigned int count_free_highmem_pages(void)
{
	struct zone *zone;
	unsigned int cnt = 0;

	for_each_zone(zone)
		if (populated_zone(zone) && is_highmem(zone))
			cnt += zone_page_state(zone, NR_FREE_PAGES);

	return cnt;
}

static struct page *saveable_highmem_page(struct zone *zone, unsigned long pfn)
{
	struct page *page;

	if (!pfn_valid(pfn))
		return NULL;

	page = pfn_to_page(pfn);
	if (page_zone(page) != zone)
		return NULL;

	BUG_ON(!PageHighMem(page));

	if (swsusp_page_is_forbidden(page) ||  swsusp_page_is_free(page) ||
	    PageReserved(page))
		return NULL;

	return page;
}


unsigned int count_highmem_pages(void)
{
	struct zone *zone;
	unsigned int n = 0;

	for_each_zone(zone) {
		unsigned long pfn, max_zone_pfn;

		if (!is_highmem(zone))
			continue;

		mark_free_pages(zone);
		max_zone_pfn = zone->zone_start_pfn + zone->spanned_pages;
		for (pfn = zone->zone_start_pfn; pfn < max_zone_pfn; pfn++)
			if (saveable_highmem_page(zone, pfn))
				n++;
	}
	return n;
}
#else
static inline void *saveable_highmem_page(struct zone *z, unsigned long p)
{
	return NULL;
}
#endif /* CONFIG_HIGHMEM */

static struct page *saveable_page(struct zone *zone, unsigned long pfn)
{
	struct page *page;

	if (!pfn_valid(pfn))
		return NULL;

	page = pfn_to_page(pfn);
	if (page_zone(page) != zone)
		return NULL;

	BUG_ON(PageHighMem(page));

	if (swsusp_page_is_forbidden(page) || swsusp_page_is_free(page))
		return NULL;

	if (PageReserved(page)
	    && (!kernel_page_present(page) || pfn_is_nosave(pfn)))
		return NULL;

	return page;
}


unsigned int count_data_pages(void)
{
	struct zone *zone;
	unsigned long pfn, max_zone_pfn;
	unsigned int n = 0;

	for_each_zone(zone) {
		if (is_highmem(zone))
			continue;

		mark_free_pages(zone);
		max_zone_pfn = zone->zone_start_pfn + zone->spanned_pages;
		for (pfn = zone->zone_start_pfn; pfn < max_zone_pfn; pfn++)
			if (saveable_page(zone, pfn))
				n++;
	}
	return n;
}

static inline void do_copy_page(long *dst, long *src)
{
	int n;

	for (n = PAGE_SIZE / sizeof(long); n; n--)
		*dst++ = *src++;
}


static void safe_copy_page(void *dst, struct page *s_page)
{
	if (kernel_page_present(s_page)) {
		do_copy_page(dst, page_address(s_page));
	} else {
		kernel_map_pages(s_page, 1, 1);
		do_copy_page(dst, page_address(s_page));
		kernel_map_pages(s_page, 1, 0);
	}
}


#ifdef CONFIG_HIGHMEM
static inline struct page *
page_is_saveable(struct zone *zone, unsigned long pfn)
{
	return is_highmem(zone) ?
		saveable_highmem_page(zone, pfn) : saveable_page(zone, pfn);
}

static void copy_data_page(unsigned long dst_pfn, unsigned long src_pfn)
{
	struct page *s_page, *d_page;
	void *src, *dst;

	s_page = pfn_to_page(src_pfn);
	d_page = pfn_to_page(dst_pfn);
	if (PageHighMem(s_page)) {
		src = kmap_atomic(s_page, KM_USER0);
		dst = kmap_atomic(d_page, KM_USER1);
		do_copy_page(dst, src);
		kunmap_atomic(src, KM_USER0);
		kunmap_atomic(dst, KM_USER1);
	} else {
		if (PageHighMem(d_page)) {
			/* Page pointed to by src may contain some kernel
			 * data modified by kmap_atomic()
			 */
			safe_copy_page(buffer, s_page);
			dst = kmap_atomic(d_page, KM_USER0);
			memcpy(dst, buffer, PAGE_SIZE);
			kunmap_atomic(dst, KM_USER0);
		} else {
			safe_copy_page(page_address(d_page), s_page);
		}
	}
}
#else
#define page_is_saveable(zone, pfn)	saveable_page(zone, pfn)

static inline void copy_data_page(unsigned long dst_pfn, unsigned long src_pfn)
{
	safe_copy_page(page_address(pfn_to_page(dst_pfn)),
				pfn_to_page(src_pfn));
}
#endif /* CONFIG_HIGHMEM */

static void
copy_data_pages(struct memory_bitmap *copy_bm, struct memory_bitmap *orig_bm)
{
	struct zone *zone;
	unsigned long pfn;

	for_each_zone(zone) {
		unsigned long max_zone_pfn;

		mark_free_pages(zone);
		max_zone_pfn = zone->zone_start_pfn + zone->spanned_pages;
		for (pfn = zone->zone_start_pfn; pfn < max_zone_pfn; pfn++)
			if (page_is_saveable(zone, pfn))
				memory_bm_set_bit(orig_bm, pfn);
	}
	memory_bm_position_reset(orig_bm);
	memory_bm_position_reset(copy_bm);
	for(;;) {
		pfn = memory_bm_next_pfn(orig_bm);
		if (unlikely(pfn == BM_END_OF_MAP))
			break;
		copy_data_page(memory_bm_next_pfn(copy_bm), pfn);
	}
}

/* Total number of image pages */
static unsigned int nr_copy_pages;
/* Number of pages needed for saving the original pfns of the image pages */
static unsigned int nr_meta_pages;


void swsusp_free(void)
{
	struct zone *zone;
	unsigned long pfn, max_zone_pfn;

	for_each_zone(zone) {
		max_zone_pfn = zone->zone_start_pfn + zone->spanned_pages;
		for (pfn = zone->zone_start_pfn; pfn < max_zone_pfn; pfn++)
			if (pfn_valid(pfn)) {
				struct page *page = pfn_to_page(pfn);

				if (swsusp_page_is_forbidden(page) &&
				    swsusp_page_is_free(page)) {
					swsusp_unset_page_forbidden(page);
					swsusp_unset_page_free(page);
					__free_page(page);
				}
			}
	}
	nr_copy_pages = 0;
	nr_meta_pages = 0;
	restore_pblist = NULL;
	buffer = NULL;
}

#ifdef CONFIG_HIGHMEM

static unsigned int count_pages_for_highmem(unsigned int nr_highmem)
{
	unsigned int free_highmem = count_free_highmem_pages();

	if (free_highmem >= nr_highmem)
		nr_highmem = 0;
	else
		nr_highmem -= free_highmem;

	return nr_highmem;
}
#else
static unsigned int
count_pages_for_highmem(unsigned int nr_highmem) { return 0; }
#endif /* CONFIG_HIGHMEM */


static int enough_free_mem(unsigned int nr_pages, unsigned int nr_highmem)
{
	struct zone *zone;
	unsigned int free = 0, meta = 0;

	for_each_zone(zone) {
		meta += snapshot_additional_pages(zone);
		if (!is_highmem(zone))
			free += zone_page_state(zone, NR_FREE_PAGES);
	}

	nr_pages += count_pages_for_highmem(nr_highmem);
	pr_debug("PM: Normal pages needed: %u + %u + %u, available pages: %u\n",
		nr_pages, PAGES_FOR_IO, meta, free);

	return free > nr_pages + PAGES_FOR_IO + meta;
}

#ifdef CONFIG_HIGHMEM

static inline int get_highmem_buffer(int safe_needed)
{
	buffer = get_image_page(GFP_ATOMIC | __GFP_COLD, safe_needed);
	return buffer ? 0 : -ENOMEM;
}


static inline unsigned int
alloc_highmem_image_pages(struct memory_bitmap *bm, unsigned int nr_highmem)
{
	unsigned int to_alloc = count_free_highmem_pages();

	if (to_alloc > nr_highmem)
		to_alloc = nr_highmem;

	nr_highmem -= to_alloc;
	while (to_alloc-- > 0) {
		struct page *page;

		page = alloc_image_page(__GFP_HIGHMEM);
		memory_bm_set_bit(bm, page_to_pfn(page));
	}
	return nr_highmem;
}
#else
static inline int get_highmem_buffer(int safe_needed) { return 0; }

static inline unsigned int
alloc_highmem_image_pages(struct memory_bitmap *bm, unsigned int n) { return 0; }
#endif /* CONFIG_HIGHMEM */


static int
swsusp_alloc(struct memory_bitmap *orig_bm, struct memory_bitmap *copy_bm,
		unsigned int nr_pages, unsigned int nr_highmem)
{
	int error;

	error = memory_bm_create(orig_bm, GFP_ATOMIC | __GFP_COLD, PG_ANY);
	if (error)
		goto Free;

	error = memory_bm_create(copy_bm, GFP_ATOMIC | __GFP_COLD, PG_ANY);
	if (error)
		goto Free;

	if (nr_highmem > 0) {
		error = get_highmem_buffer(PG_ANY);
		if (error)
			goto Free;

		nr_pages += alloc_highmem_image_pages(copy_bm, nr_highmem);
	}
	while (nr_pages-- > 0) {
		struct page *page = alloc_image_page(GFP_ATOMIC | __GFP_COLD);

		if (!page)
			goto Free;

		memory_bm_set_bit(copy_bm, page_to_pfn(page));
	}
	return 0;

 Free:
	swsusp_free();
	return -ENOMEM;
}

static struct memory_bitmap orig_bm;
static struct memory_bitmap copy_bm;

asmlinkage int swsusp_save(void)
{
	unsigned int nr_pages, nr_highmem;

	printk(KERN_INFO "PM: Creating hibernation image: \n");

	drain_local_pages(NULL);
	nr_pages = count_data_pages();
	nr_highmem = count_highmem_pages();
	printk(KERN_INFO "PM: Need to copy %u pages\n", nr_pages + nr_highmem);

	if (!enough_free_mem(nr_pages, nr_highmem)) {
		printk(KERN_ERR "PM: Not enough free memory\n");
		return -ENOMEM;
	}

	if (swsusp_alloc(&orig_bm, &copy_bm, nr_pages, nr_highmem)) {
		printk(KERN_ERR "PM: Memory allocation failed\n");
		return -ENOMEM;
	}

	/* During allocating of suspend pagedir, new cold pages may appear.
	 * Kill them.
	 */
	drain_local_pages(NULL);
	copy_data_pages(&copy_bm, &orig_bm);

	/*
	 * End of critical section. From now on, we can write to memory,
	 * but we should not touch disk. This specially means we must _not_
	 * touch swap space! Except we must write out our image of course.
	 */

	nr_pages += nr_highmem;
	nr_copy_pages = nr_pages;
	nr_meta_pages = DIV_ROUND_UP(nr_pages * sizeof(long), PAGE_SIZE);

	printk(KERN_INFO "PM: Hibernation image created (%d pages copied)\n",
		nr_pages);

	return 0;
}

#ifndef CONFIG_ARCH_HIBERNATION_HEADER
static int init_header_complete(struct swsusp_info *info)
{
	memcpy(&info->uts, init_utsname(), sizeof(struct new_utsname));
	info->version_code = LINUX_VERSION_CODE;
	return 0;
}

static char *check_image_kernel(struct swsusp_info *info)
{
	if (info->version_code != LINUX_VERSION_CODE)
		return "kernel version";
	if (strcmp(info->uts.sysname,init_utsname()->sysname))
		return "system type";
	if (strcmp(info->uts.release,init_utsname()->release))
		return "kernel release";
	if (strcmp(info->uts.version,init_utsname()->version))
		return "version";
	if (strcmp(info->uts.machine,init_utsname()->machine))
		return "machine";
	return NULL;
}
#endif /* CONFIG_ARCH_HIBERNATION_HEADER */

unsigned long snapshot_get_image_size(void)
{
	return nr_copy_pages + nr_meta_pages + 1;
}

static int init_header(struct swsusp_info *info)
{
	memset(info, 0, sizeof(struct swsusp_info));
	info->num_physpages = num_physpages;
	info->image_pages = nr_copy_pages;
	info->pages = snapshot_get_image_size();
	info->size = info->pages;
	info->size <<= PAGE_SHIFT;
	return init_header_complete(info);
}


static inline void
pack_pfns(unsigned long *buf, struct memory_bitmap *bm)
{
	int j;

	for (j = 0; j < PAGE_SIZE / sizeof(long); j++) {
		buf[j] = memory_bm_next_pfn(bm);
		if (unlikely(buf[j] == BM_END_OF_MAP))
			break;
	}
}


int snapshot_read_next(struct snapshot_handle *handle, size_t count)
{
	if (handle->cur > nr_meta_pages + nr_copy_pages)
		return 0;

	if (!buffer) {
		/* This makes the buffer be freed by swsusp_free() */
		buffer = get_image_page(GFP_ATOMIC, PG_ANY);
		if (!buffer)
			return -ENOMEM;
	}
	if (!handle->offset) {
		int error;

		error = init_header((struct swsusp_info *)buffer);
		if (error)
			return error;
		handle->buffer = buffer;
		memory_bm_position_reset(&orig_bm);
		memory_bm_position_reset(&copy_bm);
	}
	if (handle->prev < handle->cur) {
		if (handle->cur <= nr_meta_pages) {
			memset(buffer, 0, PAGE_SIZE);
			pack_pfns(buffer, &orig_bm);
		} else {
			struct page *page;

			page = pfn_to_page(memory_bm_next_pfn(&copy_bm));
			if (PageHighMem(page)) {
				/* Highmem pages are copied to the buffer,
				 * because we can't return with a kmapped
				 * highmem page (we may not be called again).
				 */
				void *kaddr;

				kaddr = kmap_atomic(page, KM_USER0);
				memcpy(buffer, kaddr, PAGE_SIZE);
				kunmap_atomic(kaddr, KM_USER0);
				handle->buffer = buffer;
			} else {
				handle->buffer = page_address(page);
			}
		}
		handle->prev = handle->cur;
	}
	handle->buf_offset = handle->cur_offset;
	if (handle->cur_offset + count >= PAGE_SIZE) {
		count = PAGE_SIZE - handle->cur_offset;
		handle->cur_offset = 0;
		handle->cur++;
	} else {
		handle->cur_offset += count;
	}
	handle->offset += count;
	return count;
}


static int mark_unsafe_pages(struct memory_bitmap *bm)
{
	struct zone *zone;
	unsigned long pfn, max_zone_pfn;

	/* Clear page flags */
	for_each_zone(zone) {
		max_zone_pfn = zone->zone_start_pfn + zone->spanned_pages;
		for (pfn = zone->zone_start_pfn; pfn < max_zone_pfn; pfn++)
			if (pfn_valid(pfn))
				swsusp_unset_page_free(pfn_to_page(pfn));
	}

	/* Mark pages that correspond to the "original" pfns as "unsafe" */
	memory_bm_position_reset(bm);
	do {
		pfn = memory_bm_next_pfn(bm);
		if (likely(pfn != BM_END_OF_MAP)) {
			if (likely(pfn_valid(pfn)))
				swsusp_set_page_free(pfn_to_page(pfn));
			else
				return -EFAULT;
		}
	} while (pfn != BM_END_OF_MAP);

	allocated_unsafe_pages = 0;

	return 0;
}

static void
duplicate_memory_bitmap(struct memory_bitmap *dst, struct memory_bitmap *src)
{
	unsigned long pfn;

	memory_bm_position_reset(src);
	pfn = memory_bm_next_pfn(src);
	while (pfn != BM_END_OF_MAP) {
		memory_bm_set_bit(dst, pfn);
		pfn = memory_bm_next_pfn(src);
	}
}

static int check_header(struct swsusp_info *info)
{
	char *reason;

	reason = check_image_kernel(info);
	if (!reason && info->num_physpages != num_physpages)
		reason = "memory size";
	if (reason) {
		printk(KERN_ERR "PM: Image mismatch: %s\n", reason);
		return -EPERM;
	}
	return 0;
}


static int
load_header(struct swsusp_info *info)
{
	int error;

	restore_pblist = NULL;
	error = check_header(info);
	if (!error) {
		nr_copy_pages = info->image_pages;
		nr_meta_pages = info->pages - info->image_pages - 1;
	}
	return error;
}

static int unpack_orig_pfns(unsigned long *buf, struct memory_bitmap *bm)
{
	int j;

	for (j = 0; j < PAGE_SIZE / sizeof(long); j++) {
		if (unlikely(buf[j] == BM_END_OF_MAP))
			break;

		if (memory_bm_pfn_present(bm, buf[j]))
			memory_bm_set_bit(bm, buf[j]);
		else
			return -EFAULT;
	}

	return 0;
}

static struct linked_page *safe_pages_list;

#ifdef CONFIG_HIGHMEM
struct highmem_pbe {
	struct page *copy_page;	/* data is here now */
	struct page *orig_page;	/* data was here before the suspend */
	struct highmem_pbe *next;
};

static struct highmem_pbe *highmem_pblist;


static unsigned int count_highmem_image_pages(struct memory_bitmap *bm)
{
	unsigned long pfn;
	unsigned int cnt = 0;

	memory_bm_position_reset(bm);
	pfn = memory_bm_next_pfn(bm);
	while (pfn != BM_END_OF_MAP) {
		if (PageHighMem(pfn_to_page(pfn)))
			cnt++;

		pfn = memory_bm_next_pfn(bm);
	}
	return cnt;
}


static unsigned int safe_highmem_pages;

static struct memory_bitmap *safe_highmem_bm;

static int
prepare_highmem_image(struct memory_bitmap *bm, unsigned int *nr_highmem_p)
{
	unsigned int to_alloc;

	if (memory_bm_create(bm, GFP_ATOMIC, PG_SAFE))
		return -ENOMEM;

	if (get_highmem_buffer(PG_SAFE))
		return -ENOMEM;

	to_alloc = count_free_highmem_pages();
	if (to_alloc > *nr_highmem_p)
		to_alloc = *nr_highmem_p;
	else
		*nr_highmem_p = to_alloc;

	safe_highmem_pages = 0;
	while (to_alloc-- > 0) {
		struct page *page;

		page = alloc_page(__GFP_HIGHMEM);
		if (!swsusp_page_is_free(page)) {
			/* The page is "safe", set its bit the bitmap */
			memory_bm_set_bit(bm, page_to_pfn(page));
			safe_highmem_pages++;
		}
		/* Mark the page as allocated */
		swsusp_set_page_forbidden(page);
		swsusp_set_page_free(page);
	}
	memory_bm_position_reset(bm);
	safe_highmem_bm = bm;
	return 0;
}


static struct page *last_highmem_page;

static void *
get_highmem_page_buffer(struct page *page, struct chain_allocator *ca)
{
	struct highmem_pbe *pbe;
	void *kaddr;

	if (swsusp_page_is_forbidden(page) && swsusp_page_is_free(page)) {
		/* We have allocated the "original" page frame and we can
		 * use it directly to store the loaded page.
		 */
		last_highmem_page = page;
		return buffer;
	}
	/* The "original" page frame has not been allocated and we have to
	 * use a "safe" page frame to store the loaded page.
	 */
	pbe = chain_alloc(ca, sizeof(struct highmem_pbe));
	if (!pbe) {
		swsusp_free();
		return ERR_PTR(-ENOMEM);
	}
	pbe->orig_page = page;
	if (safe_highmem_pages > 0) {
		struct page *tmp;

		/* Copy of the page will be stored in high memory */
		kaddr = buffer;
		tmp = pfn_to_page(memory_bm_next_pfn(safe_highmem_bm));
		safe_highmem_pages--;
		last_highmem_page = tmp;
		pbe->copy_page = tmp;
	} else {
		/* Copy of the page will be stored in normal memory */
		kaddr = safe_pages_list;
		safe_pages_list = safe_pages_list->next;
		pbe->copy_page = virt_to_page(kaddr);
	}
	pbe->next = highmem_pblist;
	highmem_pblist = pbe;
	return kaddr;
}


static void copy_last_highmem_page(void)
{
	if (last_highmem_page) {
		void *dst;

		dst = kmap_atomic(last_highmem_page, KM_USER0);
		memcpy(dst, buffer, PAGE_SIZE);
		kunmap_atomic(dst, KM_USER0);
		last_highmem_page = NULL;
	}
}

static inline int last_highmem_page_copied(void)
{
	return !last_highmem_page;
}

static inline void free_highmem_data(void)
{
	if (safe_highmem_bm)
		memory_bm_free(safe_highmem_bm, PG_UNSAFE_CLEAR);

	if (buffer)
		free_image_page(buffer, PG_UNSAFE_CLEAR);
}
#else
static inline int get_safe_write_buffer(void) { return 0; }

static unsigned int
count_highmem_image_pages(struct memory_bitmap *bm) { return 0; }

static inline int
prepare_highmem_image(struct memory_bitmap *bm, unsigned int *nr_highmem_p)
{
	return 0;
}

static inline void *
get_highmem_page_buffer(struct page *page, struct chain_allocator *ca)
{
	return ERR_PTR(-EINVAL);
}

static inline void copy_last_highmem_page(void) {}
static inline int last_highmem_page_copied(void) { return 1; }
static inline void free_highmem_data(void) {}
#endif /* CONFIG_HIGHMEM */


#define PBES_PER_LINKED_PAGE	(LINKED_PAGE_DATA_SIZE / sizeof(struct pbe))

static int
prepare_image(struct memory_bitmap *new_bm, struct memory_bitmap *bm)
{
	unsigned int nr_pages, nr_highmem;
	struct linked_page *sp_list, *lp;
	int error;

	/* If there is no highmem, the buffer will not be necessary */
	free_image_page(buffer, PG_UNSAFE_CLEAR);
	buffer = NULL;

	nr_highmem = count_highmem_image_pages(bm);
	error = mark_unsafe_pages(bm);
	if (error)
		goto Free;

	error = memory_bm_create(new_bm, GFP_ATOMIC, PG_SAFE);
	if (error)
		goto Free;

	duplicate_memory_bitmap(new_bm, bm);
	memory_bm_free(bm, PG_UNSAFE_KEEP);
	if (nr_highmem > 0) {
		error = prepare_highmem_image(bm, &nr_highmem);
		if (error)
			goto Free;
	}
	/* Reserve some safe pages for potential later use.
	 *
	 * NOTE: This way we make sure there will be enough safe pages for the
	 * chain_alloc() in get_buffer().  It is a bit wasteful, but
	 * nr_copy_pages cannot be greater than 50% of the memory anyway.
	 */
	sp_list = NULL;
	/* nr_copy_pages cannot be lesser than allocated_unsafe_pages */
	nr_pages = nr_copy_pages - nr_highmem - allocated_unsafe_pages;
	nr_pages = DIV_ROUND_UP(nr_pages, PBES_PER_LINKED_PAGE);
	while (nr_pages > 0) {
		lp = get_image_page(GFP_ATOMIC, PG_SAFE);
		if (!lp) {
			error = -ENOMEM;
			goto Free;
		}
		lp->next = sp_list;
		sp_list = lp;
		nr_pages--;
	}
	/* Preallocate memory for the image */
	safe_pages_list = NULL;
	nr_pages = nr_copy_pages - nr_highmem - allocated_unsafe_pages;
	while (nr_pages > 0) {
		lp = (struct linked_page *)get_zeroed_page(GFP_ATOMIC);
		if (!lp) {
			error = -ENOMEM;
			goto Free;
		}
		if (!swsusp_page_is_free(virt_to_page(lp))) {
			/* The page is "safe", add it to the list */
			lp->next = safe_pages_list;
			safe_pages_list = lp;
		}
		/* Mark the page as allocated */
		swsusp_set_page_forbidden(virt_to_page(lp));
		swsusp_set_page_free(virt_to_page(lp));
		nr_pages--;
	}
	/* Free the reserved safe pages so that chain_alloc() can use them */
	while (sp_list) {
		lp = sp_list->next;
		free_image_page(sp_list, PG_UNSAFE_CLEAR);
		sp_list = lp;
	}
	return 0;

 Free:
	swsusp_free();
	return error;
}


static void *get_buffer(struct memory_bitmap *bm, struct chain_allocator *ca)
{
	struct pbe *pbe;
	struct page *page;
	unsigned long pfn = memory_bm_next_pfn(bm);

	if (pfn == BM_END_OF_MAP)
		return ERR_PTR(-EFAULT);

	page = pfn_to_page(pfn);
	if (PageHighMem(page))
		return get_highmem_page_buffer(page, ca);

	if (swsusp_page_is_forbidden(page) && swsusp_page_is_free(page))
		/* We have allocated the "original" page frame and we can
		 * use it directly to store the loaded page.
		 */
		return page_address(page);

	/* The "original" page frame has not been allocated and we have to
	 * use a "safe" page frame to store the loaded page.
	 */
	pbe = chain_alloc(ca, sizeof(struct pbe));
	if (!pbe) {
		swsusp_free();
		return ERR_PTR(-ENOMEM);
	}
	pbe->orig_address = page_address(page);
	pbe->address = safe_pages_list;
	safe_pages_list = safe_pages_list->next;
	pbe->next = restore_pblist;
	restore_pblist = pbe;
	return pbe->address;
}


int snapshot_write_next(struct snapshot_handle *handle, size_t count)
{
	static struct chain_allocator ca;
	int error = 0;

	/* Check if we have already loaded the entire image */
	if (handle->prev && handle->cur > nr_meta_pages + nr_copy_pages)
		return 0;

	if (handle->offset == 0) {
		if (!buffer)
			/* This makes the buffer be freed by swsusp_free() */
			buffer = get_image_page(GFP_ATOMIC, PG_ANY);

		if (!buffer)
			return -ENOMEM;

		handle->buffer = buffer;
	}
	handle->sync_read = 1;
	if (handle->prev < handle->cur) {
		if (handle->prev == 0) {
			error = load_header(buffer);
			if (error)
				return error;

			error = memory_bm_create(&copy_bm, GFP_ATOMIC, PG_ANY);
			if (error)
				return error;

		} else if (handle->prev <= nr_meta_pages) {
			error = unpack_orig_pfns(buffer, &copy_bm);
			if (error)
				return error;

			if (handle->prev == nr_meta_pages) {
				error = prepare_image(&orig_bm, &copy_bm);
				if (error)
					return error;

				chain_init(&ca, GFP_ATOMIC, PG_SAFE);
				memory_bm_position_reset(&orig_bm);
				restore_pblist = NULL;
				handle->buffer = get_buffer(&orig_bm, &ca);
				handle->sync_read = 0;
				if (IS_ERR(handle->buffer))
					return PTR_ERR(handle->buffer);
			}
		} else {
			copy_last_highmem_page();
			handle->buffer = get_buffer(&orig_bm, &ca);
			if (IS_ERR(handle->buffer))
				return PTR_ERR(handle->buffer);
			if (handle->buffer != buffer)
				handle->sync_read = 0;
		}
		handle->prev = handle->cur;
	}
	handle->buf_offset = handle->cur_offset;
	if (handle->cur_offset + count >= PAGE_SIZE) {
		count = PAGE_SIZE - handle->cur_offset;
		handle->cur_offset = 0;
		handle->cur++;
	} else {
		handle->cur_offset += count;
	}
	handle->offset += count;
	return count;
}


void snapshot_write_finalize(struct snapshot_handle *handle)
{
	copy_last_highmem_page();
	/* Free only if we have loaded the image entirely */
	if (handle->prev && handle->cur > nr_meta_pages + nr_copy_pages) {
		memory_bm_free(&orig_bm, PG_UNSAFE_CLEAR);
		free_highmem_data();
	}
}

int snapshot_image_loaded(struct snapshot_handle *handle)
{
	return !(!nr_copy_pages || !last_highmem_page_copied() ||
			handle->cur <= nr_meta_pages + nr_copy_pages);
}

#ifdef CONFIG_HIGHMEM
/* Assumes that @buf is ready and points to a "safe" page */
static inline void
swap_two_pages_data(struct page *p1, struct page *p2, void *buf)
{
	void *kaddr1, *kaddr2;

	kaddr1 = kmap_atomic(p1, KM_USER0);
	kaddr2 = kmap_atomic(p2, KM_USER1);
	memcpy(buf, kaddr1, PAGE_SIZE);
	memcpy(kaddr1, kaddr2, PAGE_SIZE);
	memcpy(kaddr2, buf, PAGE_SIZE);
	kunmap_atomic(kaddr1, KM_USER0);
	kunmap_atomic(kaddr2, KM_USER1);
}


int restore_highmem(void)
{
	struct highmem_pbe *pbe = highmem_pblist;
	void *buf;

	if (!pbe)
		return 0;

	buf = get_image_page(GFP_ATOMIC, PG_SAFE);
	if (!buf)
		return -ENOMEM;

	while (pbe) {
		swap_two_pages_data(pbe->copy_page, pbe->orig_page, buf);
		pbe = pbe->next;
	}
	free_image_page(buf, PG_UNSAFE_CLEAR);
	return 0;
}
#endif /* CONFIG_HIGHMEM */
