
#ifndef LINUX_MM_INLINE_H
#define LINUX_MM_INLINE_H

static inline int page_is_file_cache(struct page *page)
{
	if (PageSwapBacked(page))
		return 0;

	/* The page is page cache backed by a normal filesystem. */
	return LRU_FILE;
}

static inline void
add_page_to_lru_list(struct zone *zone, struct page *page, enum lru_list l)
{
	list_add(&page->lru, &zone->lru[l].list);
	__inc_zone_state(zone, NR_LRU_BASE + l);
	mem_cgroup_add_lru_list(page, l);
}

static inline void
del_page_from_lru_list(struct zone *zone, struct page *page, enum lru_list l)
{
	list_del(&page->lru);
	__dec_zone_state(zone, NR_LRU_BASE + l);
	mem_cgroup_del_lru_list(page, l);
}

static inline void
del_page_from_lru(struct zone *zone, struct page *page)
{
	enum lru_list l = LRU_BASE;

	list_del(&page->lru);
	if (PageUnevictable(page)) {
		__ClearPageUnevictable(page);
		l = LRU_UNEVICTABLE;
	} else {
		if (PageActive(page)) {
			__ClearPageActive(page);
			l += LRU_ACTIVE;
		}
		l += page_is_file_cache(page);
	}
	__dec_zone_state(zone, NR_LRU_BASE + l);
	mem_cgroup_del_lru_list(page, l);
}

static inline enum lru_list page_lru(struct page *page)
{
	enum lru_list lru = LRU_BASE;

	if (PageUnevictable(page))
		lru = LRU_UNEVICTABLE;
	else {
		if (PageActive(page))
			lru += LRU_ACTIVE;
		lru += page_is_file_cache(page);
	}

	return lru;
}

#endif
