

#include <rdma/ib_umem.h>

#include "ehca_iverbs.h"
#include "ehca_mrmw.h"
#include "hcp_if.h"
#include "hipz_hw.h"

#define NUM_CHUNKS(length, chunk_size) \
	(((length) + (chunk_size - 1)) / (chunk_size))

/* max number of rpages (per hcall register_rpages) */
#define MAX_RPAGES 512

static struct kmem_cache *mr_cache;
static struct kmem_cache *mw_cache;

enum ehca_mr_pgsize {
	EHCA_MR_PGSIZE4K  = 0x1000L,
	EHCA_MR_PGSIZE64K = 0x10000L,
	EHCA_MR_PGSIZE1M  = 0x100000L,
	EHCA_MR_PGSIZE16M = 0x1000000L
};

#define EHCA_MR_PGSHIFT4K  12
#define EHCA_MR_PGSHIFT64K 16
#define EHCA_MR_PGSHIFT1M  20
#define EHCA_MR_PGSHIFT16M 24

static u32 ehca_encode_hwpage_size(u32 pgsize)
{
	int log = ilog2(pgsize);
	WARN_ON(log < 12 || log > 24 || log & 3);
	return (log - 12) / 4;
}

static u64 ehca_get_max_hwpage_size(struct ehca_shca *shca)
{
	return 1UL << ilog2(shca->hca_cap_mr_pgsize);
}

static struct ehca_mr *ehca_mr_new(void)
{
	struct ehca_mr *me;

	me = kmem_cache_zalloc(mr_cache, GFP_KERNEL);
	if (me)
		spin_lock_init(&me->mrlock);
	else
		ehca_gen_err("alloc failed");

	return me;
}

static void ehca_mr_delete(struct ehca_mr *me)
{
	kmem_cache_free(mr_cache, me);
}

static struct ehca_mw *ehca_mw_new(void)
{
	struct ehca_mw *me;

	me = kmem_cache_zalloc(mw_cache, GFP_KERNEL);
	if (me)
		spin_lock_init(&me->mwlock);
	else
		ehca_gen_err("alloc failed");

	return me;
}

static void ehca_mw_delete(struct ehca_mw *me)
{
	kmem_cache_free(mw_cache, me);
}

/*----------------------------------------------------------------------*/

struct ib_mr *ehca_get_dma_mr(struct ib_pd *pd, int mr_access_flags)
{
	struct ib_mr *ib_mr;
	int ret;
	struct ehca_mr *e_maxmr;
	struct ehca_pd *e_pd = container_of(pd, struct ehca_pd, ib_pd);
	struct ehca_shca *shca =
		container_of(pd->device, struct ehca_shca, ib_device);

	if (shca->maxmr) {
		e_maxmr = ehca_mr_new();
		if (!e_maxmr) {
			ehca_err(&shca->ib_device, "out of memory");
			ib_mr = ERR_PTR(-ENOMEM);
			goto get_dma_mr_exit0;
		}

		ret = ehca_reg_maxmr(shca, e_maxmr, (u64 *)KERNELBASE,
				     mr_access_flags, e_pd,
				     &e_maxmr->ib.ib_mr.lkey,
				     &e_maxmr->ib.ib_mr.rkey);
		if (ret) {
			ehca_mr_delete(e_maxmr);
			ib_mr = ERR_PTR(ret);
			goto get_dma_mr_exit0;
		}
		ib_mr = &e_maxmr->ib.ib_mr;
	} else {
		ehca_err(&shca->ib_device, "no internal max-MR exist!");
		ib_mr = ERR_PTR(-EINVAL);
		goto get_dma_mr_exit0;
	}

get_dma_mr_exit0:
	if (IS_ERR(ib_mr))
		ehca_err(&shca->ib_device, "h_ret=%li pd=%p mr_access_flags=%x",
			 PTR_ERR(ib_mr), pd, mr_access_flags);
	return ib_mr;
} /* end ehca_get_dma_mr() */

/*----------------------------------------------------------------------*/

struct ib_mr *ehca_reg_phys_mr(struct ib_pd *pd,
			       struct ib_phys_buf *phys_buf_array,
			       int num_phys_buf,
			       int mr_access_flags,
			       u64 *iova_start)
{
	struct ib_mr *ib_mr;
	int ret;
	struct ehca_mr *e_mr;
	struct ehca_shca *shca =
		container_of(pd->device, struct ehca_shca, ib_device);
	struct ehca_pd *e_pd = container_of(pd, struct ehca_pd, ib_pd);

	u64 size;

	if ((num_phys_buf <= 0) || !phys_buf_array) {
		ehca_err(pd->device, "bad input values: num_phys_buf=%x "
			 "phys_buf_array=%p", num_phys_buf, phys_buf_array);
		ib_mr = ERR_PTR(-EINVAL);
		goto reg_phys_mr_exit0;
	}
	if (((mr_access_flags & IB_ACCESS_REMOTE_WRITE) &&
	     !(mr_access_flags & IB_ACCESS_LOCAL_WRITE)) ||
	    ((mr_access_flags & IB_ACCESS_REMOTE_ATOMIC) &&
	     !(mr_access_flags & IB_ACCESS_LOCAL_WRITE))) {
		/*
		 * Remote Write Access requires Local Write Access
		 * Remote Atomic Access requires Local Write Access
		 */
		ehca_err(pd->device, "bad input values: mr_access_flags=%x",
			 mr_access_flags);
		ib_mr = ERR_PTR(-EINVAL);
		goto reg_phys_mr_exit0;
	}

	/* check physical buffer list and calculate size */
	ret = ehca_mr_chk_buf_and_calc_size(phys_buf_array, num_phys_buf,
					    iova_start, &size);
	if (ret) {
		ib_mr = ERR_PTR(ret);
		goto reg_phys_mr_exit0;
	}
	if ((size == 0) ||
	    (((u64)iova_start + size) < (u64)iova_start)) {
		ehca_err(pd->device, "bad input values: size=%llx iova_start=%p",
			 size, iova_start);
		ib_mr = ERR_PTR(-EINVAL);
		goto reg_phys_mr_exit0;
	}

	e_mr = ehca_mr_new();
	if (!e_mr) {
		ehca_err(pd->device, "out of memory");
		ib_mr = ERR_PTR(-ENOMEM);
		goto reg_phys_mr_exit0;
	}

	/* register MR on HCA */
	if (ehca_mr_is_maxmr(size, iova_start)) {
		e_mr->flags |= EHCA_MR_FLAG_MAXMR;
		ret = ehca_reg_maxmr(shca, e_mr, iova_start, mr_access_flags,
				     e_pd, &e_mr->ib.ib_mr.lkey,
				     &e_mr->ib.ib_mr.rkey);
		if (ret) {
			ib_mr = ERR_PTR(ret);
			goto reg_phys_mr_exit1;
		}
	} else {
		struct ehca_mr_pginfo pginfo;
		u32 num_kpages;
		u32 num_hwpages;
		u64 hw_pgsize;

		num_kpages = NUM_CHUNKS(((u64)iova_start % PAGE_SIZE) + size,
					PAGE_SIZE);
		/* for kernel space we try most possible pgsize */
		hw_pgsize = ehca_get_max_hwpage_size(shca);
		num_hwpages = NUM_CHUNKS(((u64)iova_start % hw_pgsize) + size,
					 hw_pgsize);
		memset(&pginfo, 0, sizeof(pginfo));
		pginfo.type = EHCA_MR_PGI_PHYS;
		pginfo.num_kpages = num_kpages;
		pginfo.hwpage_size = hw_pgsize;
		pginfo.num_hwpages = num_hwpages;
		pginfo.u.phy.num_phys_buf = num_phys_buf;
		pginfo.u.phy.phys_buf_array = phys_buf_array;
		pginfo.next_hwpage =
			((u64)iova_start & ~PAGE_MASK) / hw_pgsize;

		ret = ehca_reg_mr(shca, e_mr, iova_start, size, mr_access_flags,
				  e_pd, &pginfo, &e_mr->ib.ib_mr.lkey,
				  &e_mr->ib.ib_mr.rkey);
		if (ret) {
			ib_mr = ERR_PTR(ret);
			goto reg_phys_mr_exit1;
		}
	}

	/* successful registration of all pages */
	return &e_mr->ib.ib_mr;

reg_phys_mr_exit1:
	ehca_mr_delete(e_mr);
reg_phys_mr_exit0:
	if (IS_ERR(ib_mr))
		ehca_err(pd->device, "h_ret=%li pd=%p phys_buf_array=%p "
			 "num_phys_buf=%x mr_access_flags=%x iova_start=%p",
			 PTR_ERR(ib_mr), pd, phys_buf_array,
			 num_phys_buf, mr_access_flags, iova_start);
	return ib_mr;
} /* end ehca_reg_phys_mr() */

/*----------------------------------------------------------------------*/

struct ib_mr *ehca_reg_user_mr(struct ib_pd *pd, u64 start, u64 length,
			       u64 virt, int mr_access_flags,
			       struct ib_udata *udata)
{
	struct ib_mr *ib_mr;
	struct ehca_mr *e_mr;
	struct ehca_shca *shca =
		container_of(pd->device, struct ehca_shca, ib_device);
	struct ehca_pd *e_pd = container_of(pd, struct ehca_pd, ib_pd);
	struct ehca_mr_pginfo pginfo;
	int ret, page_shift;
	u32 num_kpages;
	u32 num_hwpages;
	u64 hwpage_size;

	if (!pd) {
		ehca_gen_err("bad pd=%p", pd);
		return ERR_PTR(-EFAULT);
	}

	if (((mr_access_flags & IB_ACCESS_REMOTE_WRITE) &&
	     !(mr_access_flags & IB_ACCESS_LOCAL_WRITE)) ||
	    ((mr_access_flags & IB_ACCESS_REMOTE_ATOMIC) &&
	     !(mr_access_flags & IB_ACCESS_LOCAL_WRITE))) {
		/*
		 * Remote Write Access requires Local Write Access
		 * Remote Atomic Access requires Local Write Access
		 */
		ehca_err(pd->device, "bad input values: mr_access_flags=%x",
			 mr_access_flags);
		ib_mr = ERR_PTR(-EINVAL);
		goto reg_user_mr_exit0;
	}

	if (length == 0 || virt + length < virt) {
		ehca_err(pd->device, "bad input values: length=%llx "
			 "virt_base=%llx", length, virt);
		ib_mr = ERR_PTR(-EINVAL);
		goto reg_user_mr_exit0;
	}

	e_mr = ehca_mr_new();
	if (!e_mr) {
		ehca_err(pd->device, "out of memory");
		ib_mr = ERR_PTR(-ENOMEM);
		goto reg_user_mr_exit0;
	}

	e_mr->umem = ib_umem_get(pd->uobject->context, start, length,
				 mr_access_flags, 0);
	if (IS_ERR(e_mr->umem)) {
		ib_mr = (void *)e_mr->umem;
		goto reg_user_mr_exit1;
	}

	if (e_mr->umem->page_size != PAGE_SIZE) {
		ehca_err(pd->device, "page size not supported, "
			 "e_mr->umem->page_size=%x", e_mr->umem->page_size);
		ib_mr = ERR_PTR(-EINVAL);
		goto reg_user_mr_exit2;
	}

	/* determine number of MR pages */
	num_kpages = NUM_CHUNKS((virt % PAGE_SIZE) + length, PAGE_SIZE);
	/* select proper hw_pgsize */
	page_shift = PAGE_SHIFT;
	if (e_mr->umem->hugetlb) {
		/* determine page_shift, clamp between 4K and 16M */
		page_shift = (fls64(length - 1) + 3) & ~3;
		page_shift = min(max(page_shift, EHCA_MR_PGSHIFT4K),
				 EHCA_MR_PGSHIFT16M);
	}
	hwpage_size = 1UL << page_shift;

	/* now that we have the desired page size, shift until it's
	 * supported, too. 4K is always supported, so this terminates.
	 */
	while (!(hwpage_size & shca->hca_cap_mr_pgsize))
		hwpage_size >>= 4;

reg_user_mr_fallback:
	num_hwpages = NUM_CHUNKS((virt % hwpage_size) + length, hwpage_size);
	/* register MR on HCA */
	memset(&pginfo, 0, sizeof(pginfo));
	pginfo.type = EHCA_MR_PGI_USER;
	pginfo.hwpage_size = hwpage_size;
	pginfo.num_kpages = num_kpages;
	pginfo.num_hwpages = num_hwpages;
	pginfo.u.usr.region = e_mr->umem;
	pginfo.next_hwpage = e_mr->umem->offset / hwpage_size;
	pginfo.u.usr.next_chunk = list_prepare_entry(pginfo.u.usr.next_chunk,
						     (&e_mr->umem->chunk_list),
						     list);

	ret = ehca_reg_mr(shca, e_mr, (u64 *)virt, length, mr_access_flags,
			  e_pd, &pginfo, &e_mr->ib.ib_mr.lkey,
			  &e_mr->ib.ib_mr.rkey);
	if (ret == -EINVAL && pginfo.hwpage_size > PAGE_SIZE) {
		ehca_warn(pd->device, "failed to register mr "
			  "with hwpage_size=%llx", hwpage_size);
		ehca_info(pd->device, "try to register mr with "
			  "kpage_size=%lx", PAGE_SIZE);
		/*
		 * this means kpages are not contiguous for a hw page
		 * try kernel page size as fallback solution
		 */
		hwpage_size = PAGE_SIZE;
		goto reg_user_mr_fallback;
	}
	if (ret) {
		ib_mr = ERR_PTR(ret);
		goto reg_user_mr_exit2;
	}

	/* successful registration of all pages */
	return &e_mr->ib.ib_mr;

reg_user_mr_exit2:
	ib_umem_release(e_mr->umem);
reg_user_mr_exit1:
	ehca_mr_delete(e_mr);
reg_user_mr_exit0:
	if (IS_ERR(ib_mr))
		ehca_err(pd->device, "rc=%li pd=%p mr_access_flags=%x udata=%p",
			 PTR_ERR(ib_mr), pd, mr_access_flags, udata);
	return ib_mr;
} /* end ehca_reg_user_mr() */

/*----------------------------------------------------------------------*/

int ehca_rereg_phys_mr(struct ib_mr *mr,
		       int mr_rereg_mask,
		       struct ib_pd *pd,
		       struct ib_phys_buf *phys_buf_array,
		       int num_phys_buf,
		       int mr_access_flags,
		       u64 *iova_start)
{
	int ret;

	struct ehca_shca *shca =
		container_of(mr->device, struct ehca_shca, ib_device);
	struct ehca_mr *e_mr = container_of(mr, struct ehca_mr, ib.ib_mr);
	u64 new_size;
	u64 *new_start;
	u32 new_acl;
	struct ehca_pd *new_pd;
	u32 tmp_lkey, tmp_rkey;
	unsigned long sl_flags;
	u32 num_kpages = 0;
	u32 num_hwpages = 0;
	struct ehca_mr_pginfo pginfo;

	if (!(mr_rereg_mask & IB_MR_REREG_TRANS)) {
		/* TODO not supported, because PHYP rereg hCall needs pages */
		ehca_err(mr->device, "rereg without IB_MR_REREG_TRANS not "
			 "supported yet, mr_rereg_mask=%x", mr_rereg_mask);
		ret = -EINVAL;
		goto rereg_phys_mr_exit0;
	}

	if (mr_rereg_mask & IB_MR_REREG_PD) {
		if (!pd) {
			ehca_err(mr->device, "rereg with bad pd, pd=%p "
				 "mr_rereg_mask=%x", pd, mr_rereg_mask);
			ret = -EINVAL;
			goto rereg_phys_mr_exit0;
		}
	}

	if ((mr_rereg_mask &
	     ~(IB_MR_REREG_TRANS | IB_MR_REREG_PD | IB_MR_REREG_ACCESS)) ||
	    (mr_rereg_mask == 0)) {
		ret = -EINVAL;
		goto rereg_phys_mr_exit0;
	}

	/* check other parameters */
	if (e_mr == shca->maxmr) {
		/* should be impossible, however reject to be sure */
		ehca_err(mr->device, "rereg internal max-MR impossible, mr=%p "
			 "shca->maxmr=%p mr->lkey=%x",
			 mr, shca->maxmr, mr->lkey);
		ret = -EINVAL;
		goto rereg_phys_mr_exit0;
	}
	if (mr_rereg_mask & IB_MR_REREG_TRANS) { /* transl., i.e. addr/size */
		if (e_mr->flags & EHCA_MR_FLAG_FMR) {
			ehca_err(mr->device, "not supported for FMR, mr=%p "
				 "flags=%x", mr, e_mr->flags);
			ret = -EINVAL;
			goto rereg_phys_mr_exit0;
		}
		if (!phys_buf_array || num_phys_buf <= 0) {
			ehca_err(mr->device, "bad input values mr_rereg_mask=%x"
				 " phys_buf_array=%p num_phys_buf=%x",
				 mr_rereg_mask, phys_buf_array, num_phys_buf);
			ret = -EINVAL;
			goto rereg_phys_mr_exit0;
		}
	}
	if ((mr_rereg_mask & IB_MR_REREG_ACCESS) &&	/* change ACL */
	    (((mr_access_flags & IB_ACCESS_REMOTE_WRITE) &&
	      !(mr_access_flags & IB_ACCESS_LOCAL_WRITE)) ||
	     ((mr_access_flags & IB_ACCESS_REMOTE_ATOMIC) &&
	      !(mr_access_flags & IB_ACCESS_LOCAL_WRITE)))) {
		/*
		 * Remote Write Access requires Local Write Access
		 * Remote Atomic Access requires Local Write Access
		 */
		ehca_err(mr->device, "bad input values: mr_rereg_mask=%x "
			 "mr_access_flags=%x", mr_rereg_mask, mr_access_flags);
		ret = -EINVAL;
		goto rereg_phys_mr_exit0;
	}

	/* set requested values dependent on rereg request */
	spin_lock_irqsave(&e_mr->mrlock, sl_flags);
	new_start = e_mr->start;
	new_size = e_mr->size;
	new_acl = e_mr->acl;
	new_pd = container_of(mr->pd, struct ehca_pd, ib_pd);

	if (mr_rereg_mask & IB_MR_REREG_TRANS) {
		u64 hw_pgsize = ehca_get_max_hwpage_size(shca);

		new_start = iova_start;	/* change address */
		/* check physical buffer list and calculate size */
		ret = ehca_mr_chk_buf_and_calc_size(phys_buf_array,
						    num_phys_buf, iova_start,
						    &new_size);
		if (ret)
			goto rereg_phys_mr_exit1;
		if ((new_size == 0) ||
		    (((u64)iova_start + new_size) < (u64)iova_start)) {
			ehca_err(mr->device, "bad input values: new_size=%llx "
				 "iova_start=%p", new_size, iova_start);
			ret = -EINVAL;
			goto rereg_phys_mr_exit1;
		}
		num_kpages = NUM_CHUNKS(((u64)new_start % PAGE_SIZE) +
					new_size, PAGE_SIZE);
		num_hwpages = NUM_CHUNKS(((u64)new_start % hw_pgsize) +
					 new_size, hw_pgsize);
		memset(&pginfo, 0, sizeof(pginfo));
		pginfo.type = EHCA_MR_PGI_PHYS;
		pginfo.num_kpages = num_kpages;
		pginfo.hwpage_size = hw_pgsize;
		pginfo.num_hwpages = num_hwpages;
		pginfo.u.phy.num_phys_buf = num_phys_buf;
		pginfo.u.phy.phys_buf_array = phys_buf_array;
		pginfo.next_hwpage =
			((u64)iova_start & ~PAGE_MASK) / hw_pgsize;
	}
	if (mr_rereg_mask & IB_MR_REREG_ACCESS)
		new_acl = mr_access_flags;
	if (mr_rereg_mask & IB_MR_REREG_PD)
		new_pd = container_of(pd, struct ehca_pd, ib_pd);

	ret = ehca_rereg_mr(shca, e_mr, new_start, new_size, new_acl,
			    new_pd, &pginfo, &tmp_lkey, &tmp_rkey);
	if (ret)
		goto rereg_phys_mr_exit1;

	/* successful reregistration */
	if (mr_rereg_mask & IB_MR_REREG_PD)
		mr->pd = pd;
	mr->lkey = tmp_lkey;
	mr->rkey = tmp_rkey;

rereg_phys_mr_exit1:
	spin_unlock_irqrestore(&e_mr->mrlock, sl_flags);
rereg_phys_mr_exit0:
	if (ret)
		ehca_err(mr->device, "ret=%i mr=%p mr_rereg_mask=%x pd=%p "
			 "phys_buf_array=%p num_phys_buf=%x mr_access_flags=%x "
			 "iova_start=%p",
			 ret, mr, mr_rereg_mask, pd, phys_buf_array,
			 num_phys_buf, mr_access_flags, iova_start);
	return ret;
} /* end ehca_rereg_phys_mr() */

/*----------------------------------------------------------------------*/

int ehca_query_mr(struct ib_mr *mr, struct ib_mr_attr *mr_attr)
{
	int ret = 0;
	u64 h_ret;
	struct ehca_shca *shca =
		container_of(mr->device, struct ehca_shca, ib_device);
	struct ehca_mr *e_mr = container_of(mr, struct ehca_mr, ib.ib_mr);
	unsigned long sl_flags;
	struct ehca_mr_hipzout_parms hipzout;

	if ((e_mr->flags & EHCA_MR_FLAG_FMR)) {
		ehca_err(mr->device, "not supported for FMR, mr=%p e_mr=%p "
			 "e_mr->flags=%x", mr, e_mr, e_mr->flags);
		ret = -EINVAL;
		goto query_mr_exit0;
	}

	memset(mr_attr, 0, sizeof(struct ib_mr_attr));
	spin_lock_irqsave(&e_mr->mrlock, sl_flags);

	h_ret = hipz_h_query_mr(shca->ipz_hca_handle, e_mr, &hipzout);
	if (h_ret != H_SUCCESS) {
		ehca_err(mr->device, "hipz_mr_query failed, h_ret=%lli mr=%p "
			 "hca_hndl=%llx mr_hndl=%llx lkey=%x",
			 h_ret, mr, shca->ipz_hca_handle.handle,
			 e_mr->ipz_mr_handle.handle, mr->lkey);
		ret = ehca2ib_return_code(h_ret);
		goto query_mr_exit1;
	}
	mr_attr->pd = mr->pd;
	mr_attr->device_virt_addr = hipzout.vaddr;
	mr_attr->size = hipzout.len;
	mr_attr->lkey = hipzout.lkey;
	mr_attr->rkey = hipzout.rkey;
	ehca_mrmw_reverse_map_acl(&hipzout.acl, &mr_attr->mr_access_flags);

query_mr_exit1:
	spin_unlock_irqrestore(&e_mr->mrlock, sl_flags);
query_mr_exit0:
	if (ret)
		ehca_err(mr->device, "ret=%i mr=%p mr_attr=%p",
			 ret, mr, mr_attr);
	return ret;
} /* end ehca_query_mr() */

/*----------------------------------------------------------------------*/

int ehca_dereg_mr(struct ib_mr *mr)
{
	int ret = 0;
	u64 h_ret;
	struct ehca_shca *shca =
		container_of(mr->device, struct ehca_shca, ib_device);
	struct ehca_mr *e_mr = container_of(mr, struct ehca_mr, ib.ib_mr);

	if ((e_mr->flags & EHCA_MR_FLAG_FMR)) {
		ehca_err(mr->device, "not supported for FMR, mr=%p e_mr=%p "
			 "e_mr->flags=%x", mr, e_mr, e_mr->flags);
		ret = -EINVAL;
		goto dereg_mr_exit0;
	} else if (e_mr == shca->maxmr) {
		/* should be impossible, however reject to be sure */
		ehca_err(mr->device, "dereg internal max-MR impossible, mr=%p "
			 "shca->maxmr=%p mr->lkey=%x",
			 mr, shca->maxmr, mr->lkey);
		ret = -EINVAL;
		goto dereg_mr_exit0;
	}

	/* TODO: BUSY: MR still has bound window(s) */
	h_ret = hipz_h_free_resource_mr(shca->ipz_hca_handle, e_mr);
	if (h_ret != H_SUCCESS) {
		ehca_err(mr->device, "hipz_free_mr failed, h_ret=%lli shca=%p "
			 "e_mr=%p hca_hndl=%llx mr_hndl=%llx mr->lkey=%x",
			 h_ret, shca, e_mr, shca->ipz_hca_handle.handle,
			 e_mr->ipz_mr_handle.handle, mr->lkey);
		ret = ehca2ib_return_code(h_ret);
		goto dereg_mr_exit0;
	}

	if (e_mr->umem)
		ib_umem_release(e_mr->umem);

	/* successful deregistration */
	ehca_mr_delete(e_mr);

dereg_mr_exit0:
	if (ret)
		ehca_err(mr->device, "ret=%i mr=%p", ret, mr);
	return ret;
} /* end ehca_dereg_mr() */

/*----------------------------------------------------------------------*/

struct ib_mw *ehca_alloc_mw(struct ib_pd *pd)
{
	struct ib_mw *ib_mw;
	u64 h_ret;
	struct ehca_mw *e_mw;
	struct ehca_pd *e_pd = container_of(pd, struct ehca_pd, ib_pd);
	struct ehca_shca *shca =
		container_of(pd->device, struct ehca_shca, ib_device);
	struct ehca_mw_hipzout_parms hipzout;

	e_mw = ehca_mw_new();
	if (!e_mw) {
		ib_mw = ERR_PTR(-ENOMEM);
		goto alloc_mw_exit0;
	}

	h_ret = hipz_h_alloc_resource_mw(shca->ipz_hca_handle, e_mw,
					 e_pd->fw_pd, &hipzout);
	if (h_ret != H_SUCCESS) {
		ehca_err(pd->device, "hipz_mw_allocate failed, h_ret=%lli "
			 "shca=%p hca_hndl=%llx mw=%p",
			 h_ret, shca, shca->ipz_hca_handle.handle, e_mw);
		ib_mw = ERR_PTR(ehca2ib_return_code(h_ret));
		goto alloc_mw_exit1;
	}
	/* successful MW allocation */
	e_mw->ipz_mw_handle = hipzout.handle;
	e_mw->ib_mw.rkey    = hipzout.rkey;
	return &e_mw->ib_mw;

alloc_mw_exit1:
	ehca_mw_delete(e_mw);
alloc_mw_exit0:
	if (IS_ERR(ib_mw))
		ehca_err(pd->device, "h_ret=%li pd=%p", PTR_ERR(ib_mw), pd);
	return ib_mw;
} /* end ehca_alloc_mw() */

/*----------------------------------------------------------------------*/

int ehca_bind_mw(struct ib_qp *qp,
		 struct ib_mw *mw,
		 struct ib_mw_bind *mw_bind)
{
	/* TODO: not supported up to now */
	ehca_gen_err("bind MW currently not supported by HCAD");

	return -EPERM;
} /* end ehca_bind_mw() */

/*----------------------------------------------------------------------*/

int ehca_dealloc_mw(struct ib_mw *mw)
{
	u64 h_ret;
	struct ehca_shca *shca =
		container_of(mw->device, struct ehca_shca, ib_device);
	struct ehca_mw *e_mw = container_of(mw, struct ehca_mw, ib_mw);

	h_ret = hipz_h_free_resource_mw(shca->ipz_hca_handle, e_mw);
	if (h_ret != H_SUCCESS) {
		ehca_err(mw->device, "hipz_free_mw failed, h_ret=%lli shca=%p "
			 "mw=%p rkey=%x hca_hndl=%llx mw_hndl=%llx",
			 h_ret, shca, mw, mw->rkey, shca->ipz_hca_handle.handle,
			 e_mw->ipz_mw_handle.handle);
		return ehca2ib_return_code(h_ret);
	}
	/* successful deallocation */
	ehca_mw_delete(e_mw);
	return 0;
} /* end ehca_dealloc_mw() */

/*----------------------------------------------------------------------*/

struct ib_fmr *ehca_alloc_fmr(struct ib_pd *pd,
			      int mr_access_flags,
			      struct ib_fmr_attr *fmr_attr)
{
	struct ib_fmr *ib_fmr;
	struct ehca_shca *shca =
		container_of(pd->device, struct ehca_shca, ib_device);
	struct ehca_pd *e_pd = container_of(pd, struct ehca_pd, ib_pd);
	struct ehca_mr *e_fmr;
	int ret;
	u32 tmp_lkey, tmp_rkey;
	struct ehca_mr_pginfo pginfo;
	u64 hw_pgsize;

	/* check other parameters */
	if (((mr_access_flags & IB_ACCESS_REMOTE_WRITE) &&
	     !(mr_access_flags & IB_ACCESS_LOCAL_WRITE)) ||
	    ((mr_access_flags & IB_ACCESS_REMOTE_ATOMIC) &&
	     !(mr_access_flags & IB_ACCESS_LOCAL_WRITE))) {
		/*
		 * Remote Write Access requires Local Write Access
		 * Remote Atomic Access requires Local Write Access
		 */
		ehca_err(pd->device, "bad input values: mr_access_flags=%x",
			 mr_access_flags);
		ib_fmr = ERR_PTR(-EINVAL);
		goto alloc_fmr_exit0;
	}
	if (mr_access_flags & IB_ACCESS_MW_BIND) {
		ehca_err(pd->device, "bad input values: mr_access_flags=%x",
			 mr_access_flags);
		ib_fmr = ERR_PTR(-EINVAL);
		goto alloc_fmr_exit0;
	}
	if ((fmr_attr->max_pages == 0) || (fmr_attr->max_maps == 0)) {
		ehca_err(pd->device, "bad input values: fmr_attr->max_pages=%x "
			 "fmr_attr->max_maps=%x fmr_attr->page_shift=%x",
			 fmr_attr->max_pages, fmr_attr->max_maps,
			 fmr_attr->page_shift);
		ib_fmr = ERR_PTR(-EINVAL);
		goto alloc_fmr_exit0;
	}

	hw_pgsize = 1 << fmr_attr->page_shift;
	if (!(hw_pgsize & shca->hca_cap_mr_pgsize)) {
		ehca_err(pd->device, "unsupported fmr_attr->page_shift=%x",
			 fmr_attr->page_shift);
		ib_fmr = ERR_PTR(-EINVAL);
		goto alloc_fmr_exit0;
	}

	e_fmr = ehca_mr_new();
	if (!e_fmr) {
		ib_fmr = ERR_PTR(-ENOMEM);
		goto alloc_fmr_exit0;
	}
	e_fmr->flags |= EHCA_MR_FLAG_FMR;

	/* register MR on HCA */
	memset(&pginfo, 0, sizeof(pginfo));
	pginfo.hwpage_size = hw_pgsize;
	/*
	 * pginfo.num_hwpages==0, ie register_rpages() will not be called
	 * but deferred to map_phys_fmr()
	 */
	ret = ehca_reg_mr(shca, e_fmr, NULL,
			  fmr_attr->max_pages * (1 << fmr_attr->page_shift),
			  mr_access_flags, e_pd, &pginfo,
			  &tmp_lkey, &tmp_rkey);
	if (ret) {
		ib_fmr = ERR_PTR(ret);
		goto alloc_fmr_exit1;
	}

	/* successful */
	e_fmr->hwpage_size = hw_pgsize;
	e_fmr->fmr_page_size = 1 << fmr_attr->page_shift;
	e_fmr->fmr_max_pages = fmr_attr->max_pages;
	e_fmr->fmr_max_maps = fmr_attr->max_maps;
	e_fmr->fmr_map_cnt = 0;
	return &e_fmr->ib.ib_fmr;

alloc_fmr_exit1:
	ehca_mr_delete(e_fmr);
alloc_fmr_exit0:
	return ib_fmr;
} /* end ehca_alloc_fmr() */

/*----------------------------------------------------------------------*/

int ehca_map_phys_fmr(struct ib_fmr *fmr,
		      u64 *page_list,
		      int list_len,
		      u64 iova)
{
	int ret;
	struct ehca_shca *shca =
		container_of(fmr->device, struct ehca_shca, ib_device);
	struct ehca_mr *e_fmr = container_of(fmr, struct ehca_mr, ib.ib_fmr);
	struct ehca_pd *e_pd = container_of(fmr->pd, struct ehca_pd, ib_pd);
	struct ehca_mr_pginfo pginfo;
	u32 tmp_lkey, tmp_rkey;

	if (!(e_fmr->flags & EHCA_MR_FLAG_FMR)) {
		ehca_err(fmr->device, "not a FMR, e_fmr=%p e_fmr->flags=%x",
			 e_fmr, e_fmr->flags);
		ret = -EINVAL;
		goto map_phys_fmr_exit0;
	}
	ret = ehca_fmr_check_page_list(e_fmr, page_list, list_len);
	if (ret)
		goto map_phys_fmr_exit0;
	if (iova % e_fmr->fmr_page_size) {
		/* only whole-numbered pages */
		ehca_err(fmr->device, "bad iova, iova=%llx fmr_page_size=%x",
			 iova, e_fmr->fmr_page_size);
		ret = -EINVAL;
		goto map_phys_fmr_exit0;
	}
	if (e_fmr->fmr_map_cnt >= e_fmr->fmr_max_maps) {
		/* HCAD does not limit the maps, however trace this anyway */
		ehca_info(fmr->device, "map limit exceeded, fmr=%p "
			  "e_fmr->fmr_map_cnt=%x e_fmr->fmr_max_maps=%x",
			  fmr, e_fmr->fmr_map_cnt, e_fmr->fmr_max_maps);
	}

	memset(&pginfo, 0, sizeof(pginfo));
	pginfo.type = EHCA_MR_PGI_FMR;
	pginfo.num_kpages = list_len;
	pginfo.hwpage_size = e_fmr->hwpage_size;
	pginfo.num_hwpages =
		list_len * e_fmr->fmr_page_size / pginfo.hwpage_size;
	pginfo.u.fmr.page_list = page_list;
	pginfo.next_hwpage =
		(iova & (e_fmr->fmr_page_size-1)) / pginfo.hwpage_size;
	pginfo.u.fmr.fmr_pgsize = e_fmr->fmr_page_size;

	ret = ehca_rereg_mr(shca, e_fmr, (u64 *)iova,
			    list_len * e_fmr->fmr_page_size,
			    e_fmr->acl, e_pd, &pginfo, &tmp_lkey, &tmp_rkey);
	if (ret)
		goto map_phys_fmr_exit0;

	/* successful reregistration */
	e_fmr->fmr_map_cnt++;
	e_fmr->ib.ib_fmr.lkey = tmp_lkey;
	e_fmr->ib.ib_fmr.rkey = tmp_rkey;
	return 0;

map_phys_fmr_exit0:
	if (ret)
		ehca_err(fmr->device, "ret=%i fmr=%p page_list=%p list_len=%x "
			 "iova=%llx", ret, fmr, page_list, list_len, iova);
	return ret;
} /* end ehca_map_phys_fmr() */

/*----------------------------------------------------------------------*/

int ehca_unmap_fmr(struct list_head *fmr_list)
{
	int ret = 0;
	struct ib_fmr *ib_fmr;
	struct ehca_shca *shca = NULL;
	struct ehca_shca *prev_shca;
	struct ehca_mr *e_fmr;
	u32 num_fmr = 0;
	u32 unmap_fmr_cnt = 0;

	/* check all FMR belong to same SHCA, and check internal flag */
	list_for_each_entry(ib_fmr, fmr_list, list) {
		prev_shca = shca;
		if (!ib_fmr) {
			ehca_gen_err("bad fmr=%p in list", ib_fmr);
			ret = -EINVAL;
			goto unmap_fmr_exit0;
		}
		shca = container_of(ib_fmr->device, struct ehca_shca,
				    ib_device);
		e_fmr = container_of(ib_fmr, struct ehca_mr, ib.ib_fmr);
		if ((shca != prev_shca) && prev_shca) {
			ehca_err(&shca->ib_device, "SHCA mismatch, shca=%p "
				 "prev_shca=%p e_fmr=%p",
				 shca, prev_shca, e_fmr);
			ret = -EINVAL;
			goto unmap_fmr_exit0;
		}
		if (!(e_fmr->flags & EHCA_MR_FLAG_FMR)) {
			ehca_err(&shca->ib_device, "not a FMR, e_fmr=%p "
				 "e_fmr->flags=%x", e_fmr, e_fmr->flags);
			ret = -EINVAL;
			goto unmap_fmr_exit0;
		}
		num_fmr++;
	}

	/* loop over all FMRs to unmap */
	list_for_each_entry(ib_fmr, fmr_list, list) {
		unmap_fmr_cnt++;
		e_fmr = container_of(ib_fmr, struct ehca_mr, ib.ib_fmr);
		shca = container_of(ib_fmr->device, struct ehca_shca,
				    ib_device);
		ret = ehca_unmap_one_fmr(shca, e_fmr);
		if (ret) {
			/* unmap failed, stop unmapping of rest of FMRs */
			ehca_err(&shca->ib_device, "unmap of one FMR failed, "
				 "stop rest, e_fmr=%p num_fmr=%x "
				 "unmap_fmr_cnt=%x lkey=%x", e_fmr, num_fmr,
				 unmap_fmr_cnt, e_fmr->ib.ib_fmr.lkey);
			goto unmap_fmr_exit0;
		}
	}

unmap_fmr_exit0:
	if (ret)
		ehca_gen_err("ret=%i fmr_list=%p num_fmr=%x unmap_fmr_cnt=%x",
			     ret, fmr_list, num_fmr, unmap_fmr_cnt);
	return ret;
} /* end ehca_unmap_fmr() */

/*----------------------------------------------------------------------*/

int ehca_dealloc_fmr(struct ib_fmr *fmr)
{
	int ret;
	u64 h_ret;
	struct ehca_shca *shca =
		container_of(fmr->device, struct ehca_shca, ib_device);
	struct ehca_mr *e_fmr = container_of(fmr, struct ehca_mr, ib.ib_fmr);

	if (!(e_fmr->flags & EHCA_MR_FLAG_FMR)) {
		ehca_err(fmr->device, "not a FMR, e_fmr=%p e_fmr->flags=%x",
			 e_fmr, e_fmr->flags);
		ret = -EINVAL;
		goto free_fmr_exit0;
	}

	h_ret = hipz_h_free_resource_mr(shca->ipz_hca_handle, e_fmr);
	if (h_ret != H_SUCCESS) {
		ehca_err(fmr->device, "hipz_free_mr failed, h_ret=%lli e_fmr=%p "
			 "hca_hndl=%llx fmr_hndl=%llx fmr->lkey=%x",
			 h_ret, e_fmr, shca->ipz_hca_handle.handle,
			 e_fmr->ipz_mr_handle.handle, fmr->lkey);
		ret = ehca2ib_return_code(h_ret);
		goto free_fmr_exit0;
	}
	/* successful deregistration */
	ehca_mr_delete(e_fmr);
	return 0;

free_fmr_exit0:
	if (ret)
		ehca_err(&shca->ib_device, "ret=%i fmr=%p", ret, fmr);
	return ret;
} /* end ehca_dealloc_fmr() */

/*----------------------------------------------------------------------*/

int ehca_reg_mr(struct ehca_shca *shca,
		struct ehca_mr *e_mr,
		u64 *iova_start,
		u64 size,
		int acl,
		struct ehca_pd *e_pd,
		struct ehca_mr_pginfo *pginfo,
		u32 *lkey, /*OUT*/
		u32 *rkey) /*OUT*/
{
	int ret;
	u64 h_ret;
	u32 hipz_acl;
	struct ehca_mr_hipzout_parms hipzout;

	ehca_mrmw_map_acl(acl, &hipz_acl);
	ehca_mrmw_set_pgsize_hipz_acl(pginfo->hwpage_size, &hipz_acl);
	if (ehca_use_hp_mr == 1)
		hipz_acl |= 0x00000001;

	h_ret = hipz_h_alloc_resource_mr(shca->ipz_hca_handle, e_mr,
					 (u64)iova_start, size, hipz_acl,
					 e_pd->fw_pd, &hipzout);
	if (h_ret != H_SUCCESS) {
		ehca_err(&shca->ib_device, "hipz_alloc_mr failed, h_ret=%lli "
			 "hca_hndl=%llx", h_ret, shca->ipz_hca_handle.handle);
		ret = ehca2ib_return_code(h_ret);
		goto ehca_reg_mr_exit0;
	}

	e_mr->ipz_mr_handle = hipzout.handle;

	ret = ehca_reg_mr_rpages(shca, e_mr, pginfo);
	if (ret)
		goto ehca_reg_mr_exit1;

	/* successful registration */
	e_mr->num_kpages = pginfo->num_kpages;
	e_mr->num_hwpages = pginfo->num_hwpages;
	e_mr->hwpage_size = pginfo->hwpage_size;
	e_mr->start = iova_start;
	e_mr->size = size;
	e_mr->acl = acl;
	*lkey = hipzout.lkey;
	*rkey = hipzout.rkey;
	return 0;

ehca_reg_mr_exit1:
	h_ret = hipz_h_free_resource_mr(shca->ipz_hca_handle, e_mr);
	if (h_ret != H_SUCCESS) {
		ehca_err(&shca->ib_device, "h_ret=%lli shca=%p e_mr=%p "
			 "iova_start=%p size=%llx acl=%x e_pd=%p lkey=%x "
			 "pginfo=%p num_kpages=%llx num_hwpages=%llx ret=%i",
			 h_ret, shca, e_mr, iova_start, size, acl, e_pd,
			 hipzout.lkey, pginfo, pginfo->num_kpages,
			 pginfo->num_hwpages, ret);
		ehca_err(&shca->ib_device, "internal error in ehca_reg_mr, "
			 "not recoverable");
	}
ehca_reg_mr_exit0:
	if (ret)
		ehca_err(&shca->ib_device, "ret=%i shca=%p e_mr=%p "
			 "iova_start=%p size=%llx acl=%x e_pd=%p pginfo=%p "
			 "num_kpages=%llx num_hwpages=%llx",
			 ret, shca, e_mr, iova_start, size, acl, e_pd, pginfo,
			 pginfo->num_kpages, pginfo->num_hwpages);
	return ret;
} /* end ehca_reg_mr() */

/*----------------------------------------------------------------------*/

int ehca_reg_mr_rpages(struct ehca_shca *shca,
		       struct ehca_mr *e_mr,
		       struct ehca_mr_pginfo *pginfo)
{
	int ret = 0;
	u64 h_ret;
	u32 rnum;
	u64 rpage;
	u32 i;
	u64 *kpage;

	if (!pginfo->num_hwpages) /* in case of fmr */
		return 0;

	kpage = ehca_alloc_fw_ctrlblock(GFP_KERNEL);
	if (!kpage) {
		ehca_err(&shca->ib_device, "kpage alloc failed");
		ret = -ENOMEM;
		goto ehca_reg_mr_rpages_exit0;
	}

	/* max MAX_RPAGES ehca mr pages per register call */
	for (i = 0; i < NUM_CHUNKS(pginfo->num_hwpages, MAX_RPAGES); i++) {

		if (i == NUM_CHUNKS(pginfo->num_hwpages, MAX_RPAGES) - 1) {
			rnum = pginfo->num_hwpages % MAX_RPAGES; /* last shot */
			if (rnum == 0)
				rnum = MAX_RPAGES;      /* last shot is full */
		} else
			rnum = MAX_RPAGES;

		ret = ehca_set_pagebuf(pginfo, rnum, kpage);
		if (ret) {
			ehca_err(&shca->ib_device, "ehca_set_pagebuf "
				 "bad rc, ret=%i rnum=%x kpage=%p",
				 ret, rnum, kpage);
			goto ehca_reg_mr_rpages_exit1;
		}

		if (rnum > 1) {
			rpage = virt_to_abs(kpage);
			if (!rpage) {
				ehca_err(&shca->ib_device, "kpage=%p i=%x",
					 kpage, i);
				ret = -EFAULT;
				goto ehca_reg_mr_rpages_exit1;
			}
		} else
			rpage = *kpage;

		h_ret = hipz_h_register_rpage_mr(
			shca->ipz_hca_handle, e_mr,
			ehca_encode_hwpage_size(pginfo->hwpage_size),
			0, rpage, rnum);

		if (i == NUM_CHUNKS(pginfo->num_hwpages, MAX_RPAGES) - 1) {
			/*
			 * check for 'registration complete'==H_SUCCESS
			 * and for 'page registered'==H_PAGE_REGISTERED
			 */
			if (h_ret != H_SUCCESS) {
				ehca_err(&shca->ib_device, "last "
					 "hipz_reg_rpage_mr failed, h_ret=%lli "
					 "e_mr=%p i=%x hca_hndl=%llx mr_hndl=%llx"
					 " lkey=%x", h_ret, e_mr, i,
					 shca->ipz_hca_handle.handle,
					 e_mr->ipz_mr_handle.handle,
					 e_mr->ib.ib_mr.lkey);
				ret = ehca2ib_return_code(h_ret);
				break;
			} else
				ret = 0;
		} else if (h_ret != H_PAGE_REGISTERED) {
			ehca_err(&shca->ib_device, "hipz_reg_rpage_mr failed, "
				 "h_ret=%lli e_mr=%p i=%x lkey=%x hca_hndl=%llx "
				 "mr_hndl=%llx", h_ret, e_mr, i,
				 e_mr->ib.ib_mr.lkey,
				 shca->ipz_hca_handle.handle,
				 e_mr->ipz_mr_handle.handle);
			ret = ehca2ib_return_code(h_ret);
			break;
		} else
			ret = 0;
	} /* end for(i) */


ehca_reg_mr_rpages_exit1:
	ehca_free_fw_ctrlblock(kpage);
ehca_reg_mr_rpages_exit0:
	if (ret)
		ehca_err(&shca->ib_device, "ret=%i shca=%p e_mr=%p pginfo=%p "
			 "num_kpages=%llx num_hwpages=%llx", ret, shca, e_mr,
			 pginfo, pginfo->num_kpages, pginfo->num_hwpages);
	return ret;
} /* end ehca_reg_mr_rpages() */

/*----------------------------------------------------------------------*/

inline int ehca_rereg_mr_rereg1(struct ehca_shca *shca,
				struct ehca_mr *e_mr,
				u64 *iova_start,
				u64 size,
				u32 acl,
				struct ehca_pd *e_pd,
				struct ehca_mr_pginfo *pginfo,
				u32 *lkey, /*OUT*/
				u32 *rkey) /*OUT*/
{
	int ret;
	u64 h_ret;
	u32 hipz_acl;
	u64 *kpage;
	u64 rpage;
	struct ehca_mr_pginfo pginfo_save;
	struct ehca_mr_hipzout_parms hipzout;

	ehca_mrmw_map_acl(acl, &hipz_acl);
	ehca_mrmw_set_pgsize_hipz_acl(pginfo->hwpage_size, &hipz_acl);

	kpage = ehca_alloc_fw_ctrlblock(GFP_KERNEL);
	if (!kpage) {
		ehca_err(&shca->ib_device, "kpage alloc failed");
		ret = -ENOMEM;
		goto ehca_rereg_mr_rereg1_exit0;
	}

	pginfo_save = *pginfo;
	ret = ehca_set_pagebuf(pginfo, pginfo->num_hwpages, kpage);
	if (ret) {
		ehca_err(&shca->ib_device, "set pagebuf failed, e_mr=%p "
			 "pginfo=%p type=%x num_kpages=%llx num_hwpages=%llx "
			 "kpage=%p", e_mr, pginfo, pginfo->type,
			 pginfo->num_kpages, pginfo->num_hwpages, kpage);
		goto ehca_rereg_mr_rereg1_exit1;
	}
	rpage = virt_to_abs(kpage);
	if (!rpage) {
		ehca_err(&shca->ib_device, "kpage=%p", kpage);
		ret = -EFAULT;
		goto ehca_rereg_mr_rereg1_exit1;
	}
	h_ret = hipz_h_reregister_pmr(shca->ipz_hca_handle, e_mr,
				      (u64)iova_start, size, hipz_acl,
				      e_pd->fw_pd, rpage, &hipzout);
	if (h_ret != H_SUCCESS) {
		/*
		 * reregistration unsuccessful, try it again with the 3 hCalls,
		 * e.g. this is required in case H_MR_CONDITION
		 * (MW bound or MR is shared)
		 */
		ehca_warn(&shca->ib_device, "hipz_h_reregister_pmr failed "
			  "(Rereg1), h_ret=%lli e_mr=%p", h_ret, e_mr);
		*pginfo = pginfo_save;
		ret = -EAGAIN;
	} else if ((u64 *)hipzout.vaddr != iova_start) {
		ehca_err(&shca->ib_device, "PHYP changed iova_start in "
			 "rereg_pmr, iova_start=%p iova_start_out=%llx e_mr=%p "
			 "mr_handle=%llx lkey=%x lkey_out=%x", iova_start,
			 hipzout.vaddr, e_mr, e_mr->ipz_mr_handle.handle,
			 e_mr->ib.ib_mr.lkey, hipzout.lkey);
		ret = -EFAULT;
	} else {
		/*
		 * successful reregistration
		 * note: start and start_out are identical for eServer HCAs
		 */
		e_mr->num_kpages = pginfo->num_kpages;
		e_mr->num_hwpages = pginfo->num_hwpages;
		e_mr->hwpage_size = pginfo->hwpage_size;
		e_mr->start = iova_start;
		e_mr->size = size;
		e_mr->acl = acl;
		*lkey = hipzout.lkey;
		*rkey = hipzout.rkey;
	}

ehca_rereg_mr_rereg1_exit1:
	ehca_free_fw_ctrlblock(kpage);
ehca_rereg_mr_rereg1_exit0:
	if ( ret && (ret != -EAGAIN) )
		ehca_err(&shca->ib_device, "ret=%i lkey=%x rkey=%x "
			 "pginfo=%p num_kpages=%llx num_hwpages=%llx",
			 ret, *lkey, *rkey, pginfo, pginfo->num_kpages,
			 pginfo->num_hwpages);
	return ret;
} /* end ehca_rereg_mr_rereg1() */

/*----------------------------------------------------------------------*/

int ehca_rereg_mr(struct ehca_shca *shca,
		  struct ehca_mr *e_mr,
		  u64 *iova_start,
		  u64 size,
		  int acl,
		  struct ehca_pd *e_pd,
		  struct ehca_mr_pginfo *pginfo,
		  u32 *lkey,
		  u32 *rkey)
{
	int ret = 0;
	u64 h_ret;
	int rereg_1_hcall = 1; /* 1: use hipz_h_reregister_pmr directly */
	int rereg_3_hcall = 0; /* 1: use 3 hipz calls for reregistration */

	/* first determine reregistration hCall(s) */
	if ((pginfo->num_hwpages > MAX_RPAGES) ||
	    (e_mr->num_hwpages > MAX_RPAGES) ||
	    (pginfo->num_hwpages > e_mr->num_hwpages)) {
		ehca_dbg(&shca->ib_device, "Rereg3 case, "
			 "pginfo->num_hwpages=%llx e_mr->num_hwpages=%x",
			 pginfo->num_hwpages, e_mr->num_hwpages);
		rereg_1_hcall = 0;
		rereg_3_hcall = 1;
	}

	if (e_mr->flags & EHCA_MR_FLAG_MAXMR) {	/* check for max-MR */
		rereg_1_hcall = 0;
		rereg_3_hcall = 1;
		e_mr->flags &= ~EHCA_MR_FLAG_MAXMR;
		ehca_err(&shca->ib_device, "Rereg MR for max-MR! e_mr=%p",
			 e_mr);
	}

	if (rereg_1_hcall) {
		ret = ehca_rereg_mr_rereg1(shca, e_mr, iova_start, size,
					   acl, e_pd, pginfo, lkey, rkey);
		if (ret) {
			if (ret == -EAGAIN)
				rereg_3_hcall = 1;
			else
				goto ehca_rereg_mr_exit0;
		}
	}

	if (rereg_3_hcall) {
		struct ehca_mr save_mr;

		/* first deregister old MR */
		h_ret = hipz_h_free_resource_mr(shca->ipz_hca_handle, e_mr);
		if (h_ret != H_SUCCESS) {
			ehca_err(&shca->ib_device, "hipz_free_mr failed, "
				 "h_ret=%lli e_mr=%p hca_hndl=%llx mr_hndl=%llx "
				 "mr->lkey=%x",
				 h_ret, e_mr, shca->ipz_hca_handle.handle,
				 e_mr->ipz_mr_handle.handle,
				 e_mr->ib.ib_mr.lkey);
			ret = ehca2ib_return_code(h_ret);
			goto ehca_rereg_mr_exit0;
		}
		/* clean ehca_mr_t, without changing struct ib_mr and lock */
		save_mr = *e_mr;
		ehca_mr_deletenew(e_mr);

		/* set some MR values */
		e_mr->flags = save_mr.flags;
		e_mr->hwpage_size = save_mr.hwpage_size;
		e_mr->fmr_page_size = save_mr.fmr_page_size;
		e_mr->fmr_max_pages = save_mr.fmr_max_pages;
		e_mr->fmr_max_maps = save_mr.fmr_max_maps;
		e_mr->fmr_map_cnt = save_mr.fmr_map_cnt;

		ret = ehca_reg_mr(shca, e_mr, iova_start, size, acl,
				  e_pd, pginfo, lkey, rkey);
		if (ret) {
			u32 offset = (u64)(&e_mr->flags) - (u64)e_mr;
			memcpy(&e_mr->flags, &(save_mr.flags),
			       sizeof(struct ehca_mr) - offset);
			goto ehca_rereg_mr_exit0;
		}
	}

ehca_rereg_mr_exit0:
	if (ret)
		ehca_err(&shca->ib_device, "ret=%i shca=%p e_mr=%p "
			 "iova_start=%p size=%llx acl=%x e_pd=%p pginfo=%p "
			 "num_kpages=%llx lkey=%x rkey=%x rereg_1_hcall=%x "
			 "rereg_3_hcall=%x", ret, shca, e_mr, iova_start, size,
			 acl, e_pd, pginfo, pginfo->num_kpages, *lkey, *rkey,
			 rereg_1_hcall, rereg_3_hcall);
	return ret;
} /* end ehca_rereg_mr() */

/*----------------------------------------------------------------------*/

int ehca_unmap_one_fmr(struct ehca_shca *shca,
		       struct ehca_mr *e_fmr)
{
	int ret = 0;
	u64 h_ret;
	struct ehca_pd *e_pd =
		container_of(e_fmr->ib.ib_fmr.pd, struct ehca_pd, ib_pd);
	struct ehca_mr save_fmr;
	u32 tmp_lkey, tmp_rkey;
	struct ehca_mr_pginfo pginfo;
	struct ehca_mr_hipzout_parms hipzout;
	struct ehca_mr save_mr;

	if (e_fmr->fmr_max_pages <= MAX_RPAGES) {
		/*
		 * note: after using rereg hcall with len=0,
		 * rereg hcall must be used again for registering pages
		 */
		h_ret = hipz_h_reregister_pmr(shca->ipz_hca_handle, e_fmr, 0,
					      0, 0, e_pd->fw_pd, 0, &hipzout);
		if (h_ret == H_SUCCESS) {
			/* successful reregistration */
			e_fmr->start = NULL;
			e_fmr->size = 0;
			tmp_lkey = hipzout.lkey;
			tmp_rkey = hipzout.rkey;
			return 0;
		}
		/*
		 * should not happen, because length checked above,
		 * FMRs are not shared and no MW bound to FMRs
		 */
		ehca_err(&shca->ib_device, "hipz_reregister_pmr failed "
			 "(Rereg1), h_ret=%lli e_fmr=%p hca_hndl=%llx "
			 "mr_hndl=%llx lkey=%x lkey_out=%x",
			 h_ret, e_fmr, shca->ipz_hca_handle.handle,
			 e_fmr->ipz_mr_handle.handle,
			 e_fmr->ib.ib_fmr.lkey, hipzout.lkey);
		/* try free and rereg */
	}

	/* first free old FMR */
	h_ret = hipz_h_free_resource_mr(shca->ipz_hca_handle, e_fmr);
	if (h_ret != H_SUCCESS) {
		ehca_err(&shca->ib_device, "hipz_free_mr failed, "
			 "h_ret=%lli e_fmr=%p hca_hndl=%llx mr_hndl=%llx "
			 "lkey=%x",
			 h_ret, e_fmr, shca->ipz_hca_handle.handle,
			 e_fmr->ipz_mr_handle.handle,
			 e_fmr->ib.ib_fmr.lkey);
		ret = ehca2ib_return_code(h_ret);
		goto ehca_unmap_one_fmr_exit0;
	}
	/* clean ehca_mr_t, without changing lock */
	save_fmr = *e_fmr;
	ehca_mr_deletenew(e_fmr);

	/* set some MR values */
	e_fmr->flags = save_fmr.flags;
	e_fmr->hwpage_size = save_fmr.hwpage_size;
	e_fmr->fmr_page_size = save_fmr.fmr_page_size;
	e_fmr->fmr_max_pages = save_fmr.fmr_max_pages;
	e_fmr->fmr_max_maps = save_fmr.fmr_max_maps;
	e_fmr->fmr_map_cnt = save_fmr.fmr_map_cnt;
	e_fmr->acl = save_fmr.acl;

	memset(&pginfo, 0, sizeof(pginfo));
	pginfo.type = EHCA_MR_PGI_FMR;
	ret = ehca_reg_mr(shca, e_fmr, NULL,
			  (e_fmr->fmr_max_pages * e_fmr->fmr_page_size),
			  e_fmr->acl, e_pd, &pginfo, &tmp_lkey,
			  &tmp_rkey);
	if (ret) {
		u32 offset = (u64)(&e_fmr->flags) - (u64)e_fmr;
		memcpy(&e_fmr->flags, &(save_mr.flags),
		       sizeof(struct ehca_mr) - offset);
	}

ehca_unmap_one_fmr_exit0:
	if (ret)
		ehca_err(&shca->ib_device, "ret=%i tmp_lkey=%x tmp_rkey=%x "
			 "fmr_max_pages=%x",
			 ret, tmp_lkey, tmp_rkey, e_fmr->fmr_max_pages);
	return ret;
} /* end ehca_unmap_one_fmr() */

/*----------------------------------------------------------------------*/

int ehca_reg_smr(struct ehca_shca *shca,
		 struct ehca_mr *e_origmr,
		 struct ehca_mr *e_newmr,
		 u64 *iova_start,
		 int acl,
		 struct ehca_pd *e_pd,
		 u32 *lkey, /*OUT*/
		 u32 *rkey) /*OUT*/
{
	int ret = 0;
	u64 h_ret;
	u32 hipz_acl;
	struct ehca_mr_hipzout_parms hipzout;

	ehca_mrmw_map_acl(acl, &hipz_acl);
	ehca_mrmw_set_pgsize_hipz_acl(e_origmr->hwpage_size, &hipz_acl);

	h_ret = hipz_h_register_smr(shca->ipz_hca_handle, e_newmr, e_origmr,
				    (u64)iova_start, hipz_acl, e_pd->fw_pd,
				    &hipzout);
	if (h_ret != H_SUCCESS) {
		ehca_err(&shca->ib_device, "hipz_reg_smr failed, h_ret=%lli "
			 "shca=%p e_origmr=%p e_newmr=%p iova_start=%p acl=%x "
			 "e_pd=%p hca_hndl=%llx mr_hndl=%llx lkey=%x",
			 h_ret, shca, e_origmr, e_newmr, iova_start, acl, e_pd,
			 shca->ipz_hca_handle.handle,
			 e_origmr->ipz_mr_handle.handle,
			 e_origmr->ib.ib_mr.lkey);
		ret = ehca2ib_return_code(h_ret);
		goto ehca_reg_smr_exit0;
	}
	/* successful registration */
	e_newmr->num_kpages = e_origmr->num_kpages;
	e_newmr->num_hwpages = e_origmr->num_hwpages;
	e_newmr->hwpage_size   = e_origmr->hwpage_size;
	e_newmr->start = iova_start;
	e_newmr->size = e_origmr->size;
	e_newmr->acl = acl;
	e_newmr->ipz_mr_handle = hipzout.handle;
	*lkey = hipzout.lkey;
	*rkey = hipzout.rkey;
	return 0;

ehca_reg_smr_exit0:
	if (ret)
		ehca_err(&shca->ib_device, "ret=%i shca=%p e_origmr=%p "
			 "e_newmr=%p iova_start=%p acl=%x e_pd=%p",
			 ret, shca, e_origmr, e_newmr, iova_start, acl, e_pd);
	return ret;
} /* end ehca_reg_smr() */

/*----------------------------------------------------------------------*/

/* register internal max-MR to internal SHCA */
int ehca_reg_internal_maxmr(
	struct ehca_shca *shca,
	struct ehca_pd *e_pd,
	struct ehca_mr **e_maxmr)  /*OUT*/
{
	int ret;
	struct ehca_mr *e_mr;
	u64 *iova_start;
	u64 size_maxmr;
	struct ehca_mr_pginfo pginfo;
	struct ib_phys_buf ib_pbuf;
	u32 num_kpages;
	u32 num_hwpages;
	u64 hw_pgsize;

	e_mr = ehca_mr_new();
	if (!e_mr) {
		ehca_err(&shca->ib_device, "out of memory");
		ret = -ENOMEM;
		goto ehca_reg_internal_maxmr_exit0;
	}
	e_mr->flags |= EHCA_MR_FLAG_MAXMR;

	/* register internal max-MR on HCA */
	size_maxmr = (u64)high_memory - PAGE_OFFSET;
	iova_start = (u64 *)KERNELBASE;
	ib_pbuf.addr = 0;
	ib_pbuf.size = size_maxmr;
	num_kpages = NUM_CHUNKS(((u64)iova_start % PAGE_SIZE) + size_maxmr,
				PAGE_SIZE);
	hw_pgsize = ehca_get_max_hwpage_size(shca);
	num_hwpages = NUM_CHUNKS(((u64)iova_start % hw_pgsize) + size_maxmr,
				 hw_pgsize);

	memset(&pginfo, 0, sizeof(pginfo));
	pginfo.type = EHCA_MR_PGI_PHYS;
	pginfo.num_kpages = num_kpages;
	pginfo.num_hwpages = num_hwpages;
	pginfo.hwpage_size = hw_pgsize;
	pginfo.u.phy.num_phys_buf = 1;
	pginfo.u.phy.phys_buf_array = &ib_pbuf;

	ret = ehca_reg_mr(shca, e_mr, iova_start, size_maxmr, 0, e_pd,
			  &pginfo, &e_mr->ib.ib_mr.lkey,
			  &e_mr->ib.ib_mr.rkey);
	if (ret) {
		ehca_err(&shca->ib_device, "reg of internal max MR failed, "
			 "e_mr=%p iova_start=%p size_maxmr=%llx num_kpages=%x "
			 "num_hwpages=%x", e_mr, iova_start, size_maxmr,
			 num_kpages, num_hwpages);
		goto ehca_reg_internal_maxmr_exit1;
	}

	/* successful registration of all pages */
	e_mr->ib.ib_mr.device = e_pd->ib_pd.device;
	e_mr->ib.ib_mr.pd = &e_pd->ib_pd;
	e_mr->ib.ib_mr.uobject = NULL;
	atomic_inc(&(e_pd->ib_pd.usecnt));
	atomic_set(&(e_mr->ib.ib_mr.usecnt), 0);
	*e_maxmr = e_mr;
	return 0;

ehca_reg_internal_maxmr_exit1:
	ehca_mr_delete(e_mr);
ehca_reg_internal_maxmr_exit0:
	if (ret)
		ehca_err(&shca->ib_device, "ret=%i shca=%p e_pd=%p e_maxmr=%p",
			 ret, shca, e_pd, e_maxmr);
	return ret;
} /* end ehca_reg_internal_maxmr() */

/*----------------------------------------------------------------------*/

int ehca_reg_maxmr(struct ehca_shca *shca,
		   struct ehca_mr *e_newmr,
		   u64 *iova_start,
		   int acl,
		   struct ehca_pd *e_pd,
		   u32 *lkey,
		   u32 *rkey)
{
	u64 h_ret;
	struct ehca_mr *e_origmr = shca->maxmr;
	u32 hipz_acl;
	struct ehca_mr_hipzout_parms hipzout;

	ehca_mrmw_map_acl(acl, &hipz_acl);
	ehca_mrmw_set_pgsize_hipz_acl(e_origmr->hwpage_size, &hipz_acl);

	h_ret = hipz_h_register_smr(shca->ipz_hca_handle, e_newmr, e_origmr,
				    (u64)iova_start, hipz_acl, e_pd->fw_pd,
				    &hipzout);
	if (h_ret != H_SUCCESS) {
		ehca_err(&shca->ib_device, "hipz_reg_smr failed, h_ret=%lli "
			 "e_origmr=%p hca_hndl=%llx mr_hndl=%llx lkey=%x",
			 h_ret, e_origmr, shca->ipz_hca_handle.handle,
			 e_origmr->ipz_mr_handle.handle,
			 e_origmr->ib.ib_mr.lkey);
		return ehca2ib_return_code(h_ret);
	}
	/* successful registration */
	e_newmr->num_kpages = e_origmr->num_kpages;
	e_newmr->num_hwpages = e_origmr->num_hwpages;
	e_newmr->hwpage_size = e_origmr->hwpage_size;
	e_newmr->start = iova_start;
	e_newmr->size = e_origmr->size;
	e_newmr->acl = acl;
	e_newmr->ipz_mr_handle = hipzout.handle;
	*lkey = hipzout.lkey;
	*rkey = hipzout.rkey;
	return 0;
} /* end ehca_reg_maxmr() */

/*----------------------------------------------------------------------*/

int ehca_dereg_internal_maxmr(struct ehca_shca *shca)
{
	int ret;
	struct ehca_mr *e_maxmr;
	struct ib_pd *ib_pd;

	if (!shca->maxmr) {
		ehca_err(&shca->ib_device, "bad call, shca=%p", shca);
		ret = -EINVAL;
		goto ehca_dereg_internal_maxmr_exit0;
	}

	e_maxmr = shca->maxmr;
	ib_pd = e_maxmr->ib.ib_mr.pd;
	shca->maxmr = NULL; /* remove internal max-MR indication from SHCA */

	ret = ehca_dereg_mr(&e_maxmr->ib.ib_mr);
	if (ret) {
		ehca_err(&shca->ib_device, "dereg internal max-MR failed, "
			 "ret=%i e_maxmr=%p shca=%p lkey=%x",
			 ret, e_maxmr, shca, e_maxmr->ib.ib_mr.lkey);
		shca->maxmr = e_maxmr;
		goto ehca_dereg_internal_maxmr_exit0;
	}

	atomic_dec(&ib_pd->usecnt);

ehca_dereg_internal_maxmr_exit0:
	if (ret)
		ehca_err(&shca->ib_device, "ret=%i shca=%p shca->maxmr=%p",
			 ret, shca, shca->maxmr);
	return ret;
} /* end ehca_dereg_internal_maxmr() */

/*----------------------------------------------------------------------*/

int ehca_mr_chk_buf_and_calc_size(struct ib_phys_buf *phys_buf_array,
				  int num_phys_buf,
				  u64 *iova_start,
				  u64 *size)
{
	struct ib_phys_buf *pbuf = phys_buf_array;
	u64 size_count = 0;
	u32 i;

	if (num_phys_buf == 0) {
		ehca_gen_err("bad phys buf array len, num_phys_buf=0");
		return -EINVAL;
	}
	/* check first buffer */
	if (((u64)iova_start & ~PAGE_MASK) != (pbuf->addr & ~PAGE_MASK)) {
		ehca_gen_err("iova_start/addr mismatch, iova_start=%p "
			     "pbuf->addr=%llx pbuf->size=%llx",
			     iova_start, pbuf->addr, pbuf->size);
		return -EINVAL;
	}
	if (((pbuf->addr + pbuf->size) % PAGE_SIZE) &&
	    (num_phys_buf > 1)) {
		ehca_gen_err("addr/size mismatch in 1st buf, pbuf->addr=%llx "
			     "pbuf->size=%llx", pbuf->addr, pbuf->size);
		return -EINVAL;
	}

	for (i = 0; i < num_phys_buf; i++) {
		if ((i > 0) && (pbuf->addr % PAGE_SIZE)) {
			ehca_gen_err("bad address, i=%x pbuf->addr=%llx "
				     "pbuf->size=%llx",
				     i, pbuf->addr, pbuf->size);
			return -EINVAL;
		}
		if (((i > 0) &&	/* not 1st */
		     (i < (num_phys_buf - 1)) &&	/* not last */
		     (pbuf->size % PAGE_SIZE)) || (pbuf->size == 0)) {
			ehca_gen_err("bad size, i=%x pbuf->size=%llx",
				     i, pbuf->size);
			return -EINVAL;
		}
		size_count += pbuf->size;
		pbuf++;
	}

	*size = size_count;
	return 0;
} /* end ehca_mr_chk_buf_and_calc_size() */

/*----------------------------------------------------------------------*/

/* check page list of map FMR verb for validness */
int ehca_fmr_check_page_list(struct ehca_mr *e_fmr,
			     u64 *page_list,
			     int list_len)
{
	u32 i;
	u64 *page;

	if ((list_len == 0) || (list_len > e_fmr->fmr_max_pages)) {
		ehca_gen_err("bad list_len, list_len=%x "
			     "e_fmr->fmr_max_pages=%x fmr=%p",
			     list_len, e_fmr->fmr_max_pages, e_fmr);
		return -EINVAL;
	}

	/* each page must be aligned */
	page = page_list;
	for (i = 0; i < list_len; i++) {
		if (*page % e_fmr->fmr_page_size) {
			ehca_gen_err("bad page, i=%x *page=%llx page=%p fmr=%p "
				     "fmr_page_size=%x", i, *page, page, e_fmr,
				     e_fmr->fmr_page_size);
			return -EINVAL;
		}
		page++;
	}

	return 0;
} /* end ehca_fmr_check_page_list() */

/*----------------------------------------------------------------------*/

/* PAGE_SIZE >= pginfo->hwpage_size */
static int ehca_set_pagebuf_user1(struct ehca_mr_pginfo *pginfo,
				  u32 number,
				  u64 *kpage)
{
	int ret = 0;
	struct ib_umem_chunk *prev_chunk;
	struct ib_umem_chunk *chunk;
	u64 pgaddr;
	u32 i = 0;
	u32 j = 0;
	int hwpages_per_kpage = PAGE_SIZE / pginfo->hwpage_size;

	/* loop over desired chunk entries */
	chunk      = pginfo->u.usr.next_chunk;
	prev_chunk = pginfo->u.usr.next_chunk;
	list_for_each_entry_continue(
		chunk, (&(pginfo->u.usr.region->chunk_list)), list) {
		for (i = pginfo->u.usr.next_nmap; i < chunk->nmap; ) {
			pgaddr = page_to_pfn(sg_page(&chunk->page_list[i]))
				<< PAGE_SHIFT ;
			*kpage = phys_to_abs(pgaddr +
					     (pginfo->next_hwpage *
					      pginfo->hwpage_size));
			if ( !(*kpage) ) {
				ehca_gen_err("pgaddr=%llx "
					     "chunk->page_list[i]=%llx "
					     "i=%x next_hwpage=%llx",
					     pgaddr, (u64)sg_dma_address(
						     &chunk->page_list[i]),
					     i, pginfo->next_hwpage);
				return -EFAULT;
			}
			(pginfo->hwpage_cnt)++;
			(pginfo->next_hwpage)++;
			kpage++;
			if (pginfo->next_hwpage % hwpages_per_kpage == 0) {
				(pginfo->kpage_cnt)++;
				(pginfo->u.usr.next_nmap)++;
				pginfo->next_hwpage = 0;
				i++;
			}
			j++;
			if (j >= number) break;
		}
		if ((pginfo->u.usr.next_nmap >= chunk->nmap) &&
		    (j >= number)) {
			pginfo->u.usr.next_nmap = 0;
			prev_chunk = chunk;
			break;
		} else if (pginfo->u.usr.next_nmap >= chunk->nmap) {
			pginfo->u.usr.next_nmap = 0;
			prev_chunk = chunk;
		} else if (j >= number)
			break;
		else
			prev_chunk = chunk;
	}
	pginfo->u.usr.next_chunk =
		list_prepare_entry(prev_chunk,
				   (&(pginfo->u.usr.region->chunk_list)),
				   list);
	return ret;
}

static int ehca_check_kpages_per_ate(struct scatterlist *page_list,
				     int start_idx, int end_idx,
				     u64 *prev_pgaddr)
{
	int t;
	for (t = start_idx; t <= end_idx; t++) {
		u64 pgaddr = page_to_pfn(sg_page(&page_list[t])) << PAGE_SHIFT;
		if (ehca_debug_level >= 3)
			ehca_gen_dbg("chunk_page=%llx value=%016llx", pgaddr,
				     *(u64 *)abs_to_virt(phys_to_abs(pgaddr)));
		if (pgaddr - PAGE_SIZE != *prev_pgaddr) {
			ehca_gen_err("uncontiguous page found pgaddr=%llx "
				     "prev_pgaddr=%llx page_list_i=%x",
				     pgaddr, *prev_pgaddr, t);
			return -EINVAL;
		}
		*prev_pgaddr = pgaddr;
	}
	return 0;
}

/* PAGE_SIZE < pginfo->hwpage_size */
static int ehca_set_pagebuf_user2(struct ehca_mr_pginfo *pginfo,
				  u32 number,
				  u64 *kpage)
{
	int ret = 0;
	struct ib_umem_chunk *prev_chunk;
	struct ib_umem_chunk *chunk;
	u64 pgaddr, prev_pgaddr;
	u32 i = 0;
	u32 j = 0;
	int kpages_per_hwpage = pginfo->hwpage_size / PAGE_SIZE;
	int nr_kpages = kpages_per_hwpage;

	/* loop over desired chunk entries */
	chunk      = pginfo->u.usr.next_chunk;
	prev_chunk = pginfo->u.usr.next_chunk;
	list_for_each_entry_continue(
		chunk, (&(pginfo->u.usr.region->chunk_list)), list) {
		for (i = pginfo->u.usr.next_nmap; i < chunk->nmap; ) {
			if (nr_kpages == kpages_per_hwpage) {
				pgaddr = ( page_to_pfn(sg_page(&chunk->page_list[i]))
					   << PAGE_SHIFT );
				*kpage = phys_to_abs(pgaddr);
				if ( !(*kpage) ) {
					ehca_gen_err("pgaddr=%llx i=%x",
						     pgaddr, i);
					ret = -EFAULT;
					return ret;
				}
				/*
				 * The first page in a hwpage must be aligned;
				 * the first MR page is exempt from this rule.
				 */
				if (pgaddr & (pginfo->hwpage_size - 1)) {
					if (pginfo->hwpage_cnt) {
						ehca_gen_err(
							"invalid alignment "
							"pgaddr=%llx i=%x "
							"mr_pgsize=%llx",
							pgaddr, i,
							pginfo->hwpage_size);
						ret = -EFAULT;
						return ret;
					}
					/* first MR page */
					pginfo->kpage_cnt =
						(pgaddr &
						 (pginfo->hwpage_size - 1)) >>
						PAGE_SHIFT;
					nr_kpages -= pginfo->kpage_cnt;
					*kpage = phys_to_abs(
						pgaddr &
						~(pginfo->hwpage_size - 1));
				}
				if (ehca_debug_level >= 3) {
					u64 val = *(u64 *)abs_to_virt(
						phys_to_abs(pgaddr));
					ehca_gen_dbg("kpage=%llx chunk_page=%llx "
						     "value=%016llx",
						     *kpage, pgaddr, val);
				}
				prev_pgaddr = pgaddr;
				i++;
				pginfo->kpage_cnt++;
				pginfo->u.usr.next_nmap++;
				nr_kpages--;
				if (!nr_kpages)
					goto next_kpage;
				continue;
			}
			if (i + nr_kpages > chunk->nmap) {
				ret = ehca_check_kpages_per_ate(
					chunk->page_list, i,
					chunk->nmap - 1, &prev_pgaddr);
				if (ret) return ret;
				pginfo->kpage_cnt += chunk->nmap - i;
				pginfo->u.usr.next_nmap += chunk->nmap - i;
				nr_kpages -= chunk->nmap - i;
				break;
			}

			ret = ehca_check_kpages_per_ate(chunk->page_list, i,
							i + nr_kpages - 1,
							&prev_pgaddr);
			if (ret) return ret;
			i += nr_kpages;
			pginfo->kpage_cnt += nr_kpages;
			pginfo->u.usr.next_nmap += nr_kpages;
next_kpage:
			nr_kpages = kpages_per_hwpage;
			(pginfo->hwpage_cnt)++;
			kpage++;
			j++;
			if (j >= number) break;
		}
		if ((pginfo->u.usr.next_nmap >= chunk->nmap) &&
		    (j >= number)) {
			pginfo->u.usr.next_nmap = 0;
			prev_chunk = chunk;
			break;
		} else if (pginfo->u.usr.next_nmap >= chunk->nmap) {
			pginfo->u.usr.next_nmap = 0;
			prev_chunk = chunk;
		} else if (j >= number)
			break;
		else
			prev_chunk = chunk;
	}
	pginfo->u.usr.next_chunk =
		list_prepare_entry(prev_chunk,
				   (&(pginfo->u.usr.region->chunk_list)),
				   list);
	return ret;
}

static int ehca_set_pagebuf_phys(struct ehca_mr_pginfo *pginfo,
				 u32 number, u64 *kpage)
{
	int ret = 0;
	struct ib_phys_buf *pbuf;
	u64 num_hw, offs_hw;
	u32 i = 0;

	/* loop over desired phys_buf_array entries */
	while (i < number) {
		pbuf   = pginfo->u.phy.phys_buf_array + pginfo->u.phy.next_buf;
		num_hw  = NUM_CHUNKS((pbuf->addr % pginfo->hwpage_size) +
				     pbuf->size, pginfo->hwpage_size);
		offs_hw = (pbuf->addr & ~(pginfo->hwpage_size - 1)) /
			pginfo->hwpage_size;
		while (pginfo->next_hwpage < offs_hw + num_hw) {
			/* sanity check */
			if ((pginfo->kpage_cnt >= pginfo->num_kpages) ||
			    (pginfo->hwpage_cnt >= pginfo->num_hwpages)) {
				ehca_gen_err("kpage_cnt >= num_kpages, "
					     "kpage_cnt=%llx num_kpages=%llx "
					     "hwpage_cnt=%llx "
					     "num_hwpages=%llx i=%x",
					     pginfo->kpage_cnt,
					     pginfo->num_kpages,
					     pginfo->hwpage_cnt,
					     pginfo->num_hwpages, i);
				return -EFAULT;
			}
			*kpage = phys_to_abs(
				(pbuf->addr & ~(pginfo->hwpage_size - 1)) +
				(pginfo->next_hwpage * pginfo->hwpage_size));
			if ( !(*kpage) && pbuf->addr ) {
				ehca_gen_err("pbuf->addr=%llx pbuf->size=%llx "
					     "next_hwpage=%llx", pbuf->addr,
					     pbuf->size, pginfo->next_hwpage);
				return -EFAULT;
			}
			(pginfo->hwpage_cnt)++;
			(pginfo->next_hwpage)++;
			if (PAGE_SIZE >= pginfo->hwpage_size) {
				if (pginfo->next_hwpage %
				    (PAGE_SIZE / pginfo->hwpage_size) == 0)
					(pginfo->kpage_cnt)++;
			} else
				pginfo->kpage_cnt += pginfo->hwpage_size /
					PAGE_SIZE;
			kpage++;
			i++;
			if (i >= number) break;
		}
		if (pginfo->next_hwpage >= offs_hw + num_hw) {
			(pginfo->u.phy.next_buf)++;
			pginfo->next_hwpage = 0;
		}
	}
	return ret;
}

static int ehca_set_pagebuf_fmr(struct ehca_mr_pginfo *pginfo,
				u32 number, u64 *kpage)
{
	int ret = 0;
	u64 *fmrlist;
	u32 i;

	/* loop over desired page_list entries */
	fmrlist = pginfo->u.fmr.page_list + pginfo->u.fmr.next_listelem;
	for (i = 0; i < number; i++) {
		*kpage = phys_to_abs((*fmrlist & ~(pginfo->hwpage_size - 1)) +
				     pginfo->next_hwpage * pginfo->hwpage_size);
		if ( !(*kpage) ) {
			ehca_gen_err("*fmrlist=%llx fmrlist=%p "
				     "next_listelem=%llx next_hwpage=%llx",
				     *fmrlist, fmrlist,
				     pginfo->u.fmr.next_listelem,
				     pginfo->next_hwpage);
			return -EFAULT;
		}
		(pginfo->hwpage_cnt)++;
		if (pginfo->u.fmr.fmr_pgsize >= pginfo->hwpage_size) {
			if (pginfo->next_hwpage %
			    (pginfo->u.fmr.fmr_pgsize /
			     pginfo->hwpage_size) == 0) {
				(pginfo->kpage_cnt)++;
				(pginfo->u.fmr.next_listelem)++;
				fmrlist++;
				pginfo->next_hwpage = 0;
			} else
				(pginfo->next_hwpage)++;
		} else {
			unsigned int cnt_per_hwpage = pginfo->hwpage_size /
				pginfo->u.fmr.fmr_pgsize;
			unsigned int j;
			u64 prev = *kpage;
			/* check if adrs are contiguous */
			for (j = 1; j < cnt_per_hwpage; j++) {
				u64 p = phys_to_abs(fmrlist[j] &
						    ~(pginfo->hwpage_size - 1));
				if (prev + pginfo->u.fmr.fmr_pgsize != p) {
					ehca_gen_err("uncontiguous fmr pages "
						     "found prev=%llx p=%llx "
						     "idx=%x", prev, p, i + j);
					return -EINVAL;
				}
				prev = p;
			}
			pginfo->kpage_cnt += cnt_per_hwpage;
			pginfo->u.fmr.next_listelem += cnt_per_hwpage;
			fmrlist += cnt_per_hwpage;
		}
		kpage++;
	}
	return ret;
}

/* setup page buffer from page info */
int ehca_set_pagebuf(struct ehca_mr_pginfo *pginfo,
		     u32 number,
		     u64 *kpage)
{
	int ret;

	switch (pginfo->type) {
	case EHCA_MR_PGI_PHYS:
		ret = ehca_set_pagebuf_phys(pginfo, number, kpage);
		break;
	case EHCA_MR_PGI_USER:
		ret = PAGE_SIZE >= pginfo->hwpage_size ?
			ehca_set_pagebuf_user1(pginfo, number, kpage) :
			ehca_set_pagebuf_user2(pginfo, number, kpage);
		break;
	case EHCA_MR_PGI_FMR:
		ret = ehca_set_pagebuf_fmr(pginfo, number, kpage);
		break;
	default:
		ehca_gen_err("bad pginfo->type=%x", pginfo->type);
		ret = -EFAULT;
		break;
	}
	return ret;
} /* end ehca_set_pagebuf() */

/*----------------------------------------------------------------------*/

int ehca_mr_is_maxmr(u64 size,
		     u64 *iova_start)
{
	/* a MR is treated as max-MR only if it fits following: */
	if ((size == ((u64)high_memory - PAGE_OFFSET)) &&
	    (iova_start == (void *)KERNELBASE)) {
		ehca_gen_dbg("this is a max-MR");
		return 1;
	} else
		return 0;
} /* end ehca_mr_is_maxmr() */

/*----------------------------------------------------------------------*/

/* map access control for MR/MW. This routine is used for MR and MW. */
void ehca_mrmw_map_acl(int ib_acl,
		       u32 *hipz_acl)
{
	*hipz_acl = 0;
	if (ib_acl & IB_ACCESS_REMOTE_READ)
		*hipz_acl |= HIPZ_ACCESSCTRL_R_READ;
	if (ib_acl & IB_ACCESS_REMOTE_WRITE)
		*hipz_acl |= HIPZ_ACCESSCTRL_R_WRITE;
	if (ib_acl & IB_ACCESS_REMOTE_ATOMIC)
		*hipz_acl |= HIPZ_ACCESSCTRL_R_ATOMIC;
	if (ib_acl & IB_ACCESS_LOCAL_WRITE)
		*hipz_acl |= HIPZ_ACCESSCTRL_L_WRITE;
	if (ib_acl & IB_ACCESS_MW_BIND)
		*hipz_acl |= HIPZ_ACCESSCTRL_MW_BIND;
} /* end ehca_mrmw_map_acl() */

/*----------------------------------------------------------------------*/

/* sets page size in hipz access control for MR/MW. */
void ehca_mrmw_set_pgsize_hipz_acl(u32 pgsize, u32 *hipz_acl) /*INOUT*/
{
	*hipz_acl |= (ehca_encode_hwpage_size(pgsize) << 24);
} /* end ehca_mrmw_set_pgsize_hipz_acl() */

/*----------------------------------------------------------------------*/

void ehca_mrmw_reverse_map_acl(const u32 *hipz_acl,
			       int *ib_acl) /*OUT*/
{
	*ib_acl = 0;
	if (*hipz_acl & HIPZ_ACCESSCTRL_R_READ)
		*ib_acl |= IB_ACCESS_REMOTE_READ;
	if (*hipz_acl & HIPZ_ACCESSCTRL_R_WRITE)
		*ib_acl |= IB_ACCESS_REMOTE_WRITE;
	if (*hipz_acl & HIPZ_ACCESSCTRL_R_ATOMIC)
		*ib_acl |= IB_ACCESS_REMOTE_ATOMIC;
	if (*hipz_acl & HIPZ_ACCESSCTRL_L_WRITE)
		*ib_acl |= IB_ACCESS_LOCAL_WRITE;
	if (*hipz_acl & HIPZ_ACCESSCTRL_MW_BIND)
		*ib_acl |= IB_ACCESS_MW_BIND;
} /* end ehca_mrmw_reverse_map_acl() */


/*----------------------------------------------------------------------*/

void ehca_mr_deletenew(struct ehca_mr *mr)
{
	mr->flags = 0;
	mr->num_kpages = 0;
	mr->num_hwpages = 0;
	mr->acl = 0;
	mr->start = NULL;
	mr->fmr_page_size = 0;
	mr->fmr_max_pages = 0;
	mr->fmr_max_maps = 0;
	mr->fmr_map_cnt = 0;
	memset(&mr->ipz_mr_handle, 0, sizeof(mr->ipz_mr_handle));
	memset(&mr->galpas, 0, sizeof(mr->galpas));
} /* end ehca_mr_deletenew() */

int ehca_init_mrmw_cache(void)
{
	mr_cache = kmem_cache_create("ehca_cache_mr",
				     sizeof(struct ehca_mr), 0,
				     SLAB_HWCACHE_ALIGN,
				     NULL);
	if (!mr_cache)
		return -ENOMEM;
	mw_cache = kmem_cache_create("ehca_cache_mw",
				     sizeof(struct ehca_mw), 0,
				     SLAB_HWCACHE_ALIGN,
				     NULL);
	if (!mw_cache) {
		kmem_cache_destroy(mr_cache);
		mr_cache = NULL;
		return -ENOMEM;
	}
	return 0;
}

void ehca_cleanup_mrmw_cache(void)
{
	if (mr_cache)
		kmem_cache_destroy(mr_cache);
	if (mw_cache)
		kmem_cache_destroy(mw_cache);
}
