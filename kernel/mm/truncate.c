

#include <linux/kernel.h>
#include <linux/backing-dev.h>
#include <linux/mm.h>
#include <linux/swap.h>
#include <linux/module.h>
#include <linux/pagemap.h>
#include <linux/highmem.h>
#include <linux/pagevec.h>
#include <linux/task_io_accounting_ops.h>
#include <linux/buffer_head.h>	/* grr. try_to_release_page,
				   do_invalidatepage */
#include "internal.h"


void do_invalidatepage(struct page *page, unsigned long offset)
{
	void (*invalidatepage)(struct page *, unsigned long);
	invalidatepage = page->mapping->a_ops->invalidatepage;
#ifdef CONFIG_BLOCK
	if (!invalidatepage)
		invalidatepage = block_invalidatepage;
#endif
	if (invalidatepage)
		(*invalidatepage)(page, offset);
}

static inline void truncate_partial_page(struct page *page, unsigned partial)
{
	zero_user_segment(page, partial, PAGE_CACHE_SIZE);
	if (PagePrivate(page))
		do_invalidatepage(page, partial);
}

void cancel_dirty_page(struct page *page, unsigned int account_size)
{
	if (TestClearPageDirty(page)) {
		struct address_space *mapping = page->mapping;
		if (mapping && mapping_cap_account_dirty(mapping)) {
			dec_zone_page_state(page, NR_FILE_DIRTY);
			dec_bdi_stat(mapping->backing_dev_info,
					BDI_RECLAIMABLE);
			if (account_size)
				task_io_account_cancelled_write(account_size);
		}
	}
}
EXPORT_SYMBOL(cancel_dirty_page);

static void
truncate_complete_page(struct address_space *mapping, struct page *page)
{
	if (page->mapping != mapping)
		return;

	if (PagePrivate(page))
		do_invalidatepage(page, 0);

	cancel_dirty_page(page, PAGE_CACHE_SIZE);

	clear_page_mlock(page);
	remove_from_page_cache(page);
	ClearPageMappedToDisk(page);
	page_cache_release(page);	/* pagecache ref */
}

static int
invalidate_complete_page(struct address_space *mapping, struct page *page)
{
	int ret;

	if (page->mapping != mapping)
		return 0;

	if (PagePrivate(page) && !try_to_release_page(page, 0))
		return 0;

	clear_page_mlock(page);
	ret = remove_mapping(mapping, page);

	return ret;
}

void truncate_inode_pages_range(struct address_space *mapping,
				loff_t lstart, loff_t lend)
{
	const pgoff_t start = (lstart + PAGE_CACHE_SIZE-1) >> PAGE_CACHE_SHIFT;
	pgoff_t end;
	const unsigned partial = lstart & (PAGE_CACHE_SIZE - 1);
	struct pagevec pvec;
	pgoff_t next;
	int i;

	if (mapping->nrpages == 0)
		return;

	BUG_ON((lend & (PAGE_CACHE_SIZE - 1)) != (PAGE_CACHE_SIZE - 1));
	end = (lend >> PAGE_CACHE_SHIFT);

	pagevec_init(&pvec, 0);
	next = start;
	while (next <= end &&
	       pagevec_lookup(&pvec, mapping, next, PAGEVEC_SIZE)) {
		for (i = 0; i < pagevec_count(&pvec); i++) {
			struct page *page = pvec.pages[i];
			pgoff_t page_index = page->index;

			if (page_index > end) {
				next = page_index;
				break;
			}

			if (page_index > next)
				next = page_index;
			next++;
			if (!trylock_page(page))
				continue;
			if (PageWriteback(page)) {
				unlock_page(page);
				continue;
			}
			if (page_mapped(page)) {
				unmap_mapping_range(mapping,
				  (loff_t)page_index<<PAGE_CACHE_SHIFT,
				  PAGE_CACHE_SIZE, 0);
			}
			truncate_complete_page(mapping, page);
			unlock_page(page);
		}
		pagevec_release(&pvec);
		cond_resched();
	}

	if (partial) {
		struct page *page = find_lock_page(mapping, start - 1);
		if (page) {
			wait_on_page_writeback(page);
			truncate_partial_page(page, partial);
			unlock_page(page);
			page_cache_release(page);
		}
	}

	next = start;
	for ( ; ; ) {
		cond_resched();
		if (!pagevec_lookup(&pvec, mapping, next, PAGEVEC_SIZE)) {
			if (next == start)
				break;
			next = start;
			continue;
		}
		if (pvec.pages[0]->index > end) {
			pagevec_release(&pvec);
			break;
		}
		for (i = 0; i < pagevec_count(&pvec); i++) {
			struct page *page = pvec.pages[i];

			if (page->index > end)
				break;
			lock_page(page);
			wait_on_page_writeback(page);
			if (page_mapped(page)) {
				unmap_mapping_range(mapping,
				  (loff_t)page->index<<PAGE_CACHE_SHIFT,
				  PAGE_CACHE_SIZE, 0);
			}
			if (page->index > next)
				next = page->index;
			next++;
			truncate_complete_page(mapping, page);
			unlock_page(page);
		}
		pagevec_release(&pvec);
	}
}
EXPORT_SYMBOL(truncate_inode_pages_range);

void truncate_inode_pages(struct address_space *mapping, loff_t lstart)
{
	truncate_inode_pages_range(mapping, lstart, (loff_t)-1);
}
EXPORT_SYMBOL(truncate_inode_pages);

unsigned long __invalidate_mapping_pages(struct address_space *mapping,
				pgoff_t start, pgoff_t end, bool be_atomic)
{
	struct pagevec pvec;
	pgoff_t next = start;
	unsigned long ret = 0;
	int i;

	pagevec_init(&pvec, 0);
	while (next <= end &&
			pagevec_lookup(&pvec, mapping, next, PAGEVEC_SIZE)) {
		for (i = 0; i < pagevec_count(&pvec); i++) {
			struct page *page = pvec.pages[i];
			pgoff_t index;
			int lock_failed;

			lock_failed = !trylock_page(page);

			/*
			 * We really shouldn't be looking at the ->index of an
			 * unlocked page.  But we're not allowed to lock these
			 * pages.  So we rely upon nobody altering the ->index
			 * of this (pinned-by-us) page.
			 */
			index = page->index;
			if (index > next)
				next = index;
			next++;
			if (lock_failed)
				continue;

			if (PageDirty(page) || PageWriteback(page))
				goto unlock;
			if (page_mapped(page))
				goto unlock;
			ret += invalidate_complete_page(mapping, page);
unlock:
			unlock_page(page);
			if (next > end)
				break;
		}
		pagevec_release(&pvec);
		if (likely(!be_atomic))
			cond_resched();
	}
	return ret;
}

unsigned long invalidate_mapping_pages(struct address_space *mapping,
				pgoff_t start, pgoff_t end)
{
	return __invalidate_mapping_pages(mapping, start, end, false);
}
EXPORT_SYMBOL(invalidate_mapping_pages);

static int
invalidate_complete_page2(struct address_space *mapping, struct page *page)
{
	if (page->mapping != mapping)
		return 0;

	if (PagePrivate(page) && !try_to_release_page(page, GFP_KERNEL))
		return 0;

	spin_lock_irq(&mapping->tree_lock);
	if (PageDirty(page))
		goto failed;

	clear_page_mlock(page);
	BUG_ON(PagePrivate(page));
	__remove_from_page_cache(page);
	spin_unlock_irq(&mapping->tree_lock);
	page_cache_release(page);	/* pagecache ref */
	return 1;
failed:
	spin_unlock_irq(&mapping->tree_lock);
	return 0;
}

static int do_launder_page(struct address_space *mapping, struct page *page)
{
	if (!PageDirty(page))
		return 0;
	if (page->mapping != mapping || mapping->a_ops->launder_page == NULL)
		return 0;
	return mapping->a_ops->launder_page(page);
}

int invalidate_inode_pages2_range(struct address_space *mapping,
				  pgoff_t start, pgoff_t end)
{
	struct pagevec pvec;
	pgoff_t next;
	int i;
	int ret = 0;
	int ret2 = 0;
	int did_range_unmap = 0;
	int wrapped = 0;

	pagevec_init(&pvec, 0);
	next = start;
	while (next <= end && !wrapped &&
		pagevec_lookup(&pvec, mapping, next,
			min(end - next, (pgoff_t)PAGEVEC_SIZE - 1) + 1)) {
		for (i = 0; i < pagevec_count(&pvec); i++) {
			struct page *page = pvec.pages[i];
			pgoff_t page_index;

			lock_page(page);
			if (page->mapping != mapping) {
				unlock_page(page);
				continue;
			}
			page_index = page->index;
			next = page_index + 1;
			if (next == 0)
				wrapped = 1;
			if (page_index > end) {
				unlock_page(page);
				break;
			}
			wait_on_page_writeback(page);
			if (page_mapped(page)) {
				if (!did_range_unmap) {
					/*
					 * Zap the rest of the file in one hit.
					 */
					unmap_mapping_range(mapping,
					   (loff_t)page_index<<PAGE_CACHE_SHIFT,
					   (loff_t)(end - page_index + 1)
							<< PAGE_CACHE_SHIFT,
					    0);
					did_range_unmap = 1;
				} else {
					/*
					 * Just zap this page
					 */
					unmap_mapping_range(mapping,
					  (loff_t)page_index<<PAGE_CACHE_SHIFT,
					  PAGE_CACHE_SIZE, 0);
				}
			}
			BUG_ON(page_mapped(page));
			ret2 = do_launder_page(mapping, page);
			if (ret2 == 0) {
				if (!invalidate_complete_page2(mapping, page))
					ret2 = -EBUSY;
			}
			if (ret2 < 0)
				ret = ret2;
			unlock_page(page);
		}
		pagevec_release(&pvec);
		cond_resched();
	}
	return ret;
}
EXPORT_SYMBOL_GPL(invalidate_inode_pages2_range);

int invalidate_inode_pages2(struct address_space *mapping)
{
	return invalidate_inode_pages2_range(mapping, 0, -1);
}
EXPORT_SYMBOL_GPL(invalidate_inode_pages2);
