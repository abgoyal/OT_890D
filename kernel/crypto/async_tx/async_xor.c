
#include <linux/kernel.h>
#include <linux/interrupt.h>
#include <linux/mm.h>
#include <linux/dma-mapping.h>
#include <linux/raid/xor.h>
#include <linux/async_tx.h>

static __always_inline struct dma_async_tx_descriptor *
do_async_xor(struct dma_chan *chan, struct page *dest, struct page **src_list,
	     unsigned int offset, int src_cnt, size_t len,
	     enum async_tx_flags flags,
	     struct dma_async_tx_descriptor *depend_tx,
	     dma_async_tx_callback cb_fn, void *cb_param)
{
	struct dma_device *dma = chan->device;
	dma_addr_t *dma_src = (dma_addr_t *) src_list;
	struct dma_async_tx_descriptor *tx = NULL;
	int src_off = 0;
	int i;
	dma_async_tx_callback _cb_fn;
	void *_cb_param;
	enum async_tx_flags async_flags;
	enum dma_ctrl_flags dma_flags;
	int xor_src_cnt;
	dma_addr_t dma_dest;

	/* map the dest bidrectional in case it is re-used as a source */
	dma_dest = dma_map_page(dma->dev, dest, offset, len, DMA_BIDIRECTIONAL);
	for (i = 0; i < src_cnt; i++) {
		/* only map the dest once */
		if (unlikely(src_list[i] == dest)) {
			dma_src[i] = dma_dest;
			continue;
		}
		dma_src[i] = dma_map_page(dma->dev, src_list[i], offset,
					  len, DMA_TO_DEVICE);
	}

	while (src_cnt) {
		async_flags = flags;
		dma_flags = 0;
		xor_src_cnt = min(src_cnt, dma->max_xor);
		/* if we are submitting additional xors, leave the chain open,
		 * clear the callback parameters, and leave the destination
		 * buffer mapped
		 */
		if (src_cnt > xor_src_cnt) {
			async_flags &= ~ASYNC_TX_ACK;
			dma_flags = DMA_COMPL_SKIP_DEST_UNMAP;
			_cb_fn = NULL;
			_cb_param = NULL;
		} else {
			_cb_fn = cb_fn;
			_cb_param = cb_param;
		}
		if (_cb_fn)
			dma_flags |= DMA_PREP_INTERRUPT;

		/* Since we have clobbered the src_list we are committed
		 * to doing this asynchronously.  Drivers force forward progress
		 * in case they can not provide a descriptor
		 */
		tx = dma->device_prep_dma_xor(chan, dma_dest, &dma_src[src_off],
					      xor_src_cnt, len, dma_flags);

		if (unlikely(!tx))
			async_tx_quiesce(&depend_tx);

		/* spin wait for the preceeding transactions to complete */
		while (unlikely(!tx)) {
			dma_async_issue_pending(chan);
			tx = dma->device_prep_dma_xor(chan, dma_dest,
						      &dma_src[src_off],
						      xor_src_cnt, len,
						      dma_flags);
		}

		async_tx_submit(chan, tx, async_flags, depend_tx, _cb_fn,
				_cb_param);

		depend_tx = tx;
		flags |= ASYNC_TX_DEP_ACK;

		if (src_cnt > xor_src_cnt) {
			/* drop completed sources */
			src_cnt -= xor_src_cnt;
			src_off += xor_src_cnt;

			/* use the intermediate result a source */
			dma_src[--src_off] = dma_dest;
			src_cnt++;
		} else
			break;
	}

	return tx;
}

static void
do_sync_xor(struct page *dest, struct page **src_list, unsigned int offset,
	    int src_cnt, size_t len, enum async_tx_flags flags,
	    dma_async_tx_callback cb_fn, void *cb_param)
{
	int i;
	int xor_src_cnt;
	int src_off = 0;
	void *dest_buf;
	void **srcs = (void **) src_list;

	/* reuse the 'src_list' array to convert to buffer pointers */
	for (i = 0; i < src_cnt; i++)
		srcs[i] = page_address(src_list[i]) + offset;

	/* set destination address */
	dest_buf = page_address(dest) + offset;

	if (flags & ASYNC_TX_XOR_ZERO_DST)
		memset(dest_buf, 0, len);

	while (src_cnt > 0) {
		/* process up to 'MAX_XOR_BLOCKS' sources */
		xor_src_cnt = min(src_cnt, MAX_XOR_BLOCKS);
		xor_blocks(xor_src_cnt, len, dest_buf, &srcs[src_off]);

		/* drop completed sources */
		src_cnt -= xor_src_cnt;
		src_off += xor_src_cnt;
	}

	async_tx_sync_epilog(cb_fn, cb_param);
}

struct dma_async_tx_descriptor *
async_xor(struct page *dest, struct page **src_list, unsigned int offset,
	int src_cnt, size_t len, enum async_tx_flags flags,
	struct dma_async_tx_descriptor *depend_tx,
	dma_async_tx_callback cb_fn, void *cb_param)
{
	struct dma_chan *chan = async_tx_find_channel(depend_tx, DMA_XOR,
						      &dest, 1, src_list,
						      src_cnt, len);
	BUG_ON(src_cnt <= 1);

	if (chan) {
		/* run the xor asynchronously */
		pr_debug("%s (async): len: %zu\n", __func__, len);

		return do_async_xor(chan, dest, src_list, offset, src_cnt, len,
				    flags, depend_tx, cb_fn, cb_param);
	} else {
		/* run the xor synchronously */
		pr_debug("%s (sync): len: %zu\n", __func__, len);

		/* in the sync case the dest is an implied source
		 * (assumes the dest is the first source)
		 */
		if (flags & ASYNC_TX_XOR_DROP_DST) {
			src_cnt--;
			src_list++;
		}

		/* wait for any prerequisite operations */
		async_tx_quiesce(&depend_tx);

		do_sync_xor(dest, src_list, offset, src_cnt, len,
			    flags, cb_fn, cb_param);

		return NULL;
	}
}
EXPORT_SYMBOL_GPL(async_xor);

static int page_is_zero(struct page *p, unsigned int offset, size_t len)
{
	char *a = page_address(p) + offset;
	return ((*(u32 *) a) == 0 &&
		memcmp(a, a + 4, len - 4) == 0);
}

struct dma_async_tx_descriptor *
async_xor_zero_sum(struct page *dest, struct page **src_list,
	unsigned int offset, int src_cnt, size_t len,
	u32 *result, enum async_tx_flags flags,
	struct dma_async_tx_descriptor *depend_tx,
	dma_async_tx_callback cb_fn, void *cb_param)
{
	struct dma_chan *chan = async_tx_find_channel(depend_tx, DMA_ZERO_SUM,
						      &dest, 1, src_list,
						      src_cnt, len);
	struct dma_device *device = chan ? chan->device : NULL;
	struct dma_async_tx_descriptor *tx = NULL;

	BUG_ON(src_cnt <= 1);

	if (device && src_cnt <= device->max_xor) {
		dma_addr_t *dma_src = (dma_addr_t *) src_list;
		unsigned long dma_prep_flags = cb_fn ? DMA_PREP_INTERRUPT : 0;
		int i;

		pr_debug("%s: (async) len: %zu\n", __func__, len);

		for (i = 0; i < src_cnt; i++)
			dma_src[i] = dma_map_page(device->dev, src_list[i],
						  offset, len, DMA_TO_DEVICE);

		tx = device->device_prep_dma_zero_sum(chan, dma_src, src_cnt,
						      len, result,
						      dma_prep_flags);
		if (unlikely(!tx)) {
			async_tx_quiesce(&depend_tx);

			while (!tx) {
				dma_async_issue_pending(chan);
				tx = device->device_prep_dma_zero_sum(chan,
					dma_src, src_cnt, len, result,
					dma_prep_flags);
			}
		}

		async_tx_submit(chan, tx, flags, depend_tx, cb_fn, cb_param);
	} else {
		unsigned long xor_flags = flags;

		pr_debug("%s: (sync) len: %zu\n", __func__, len);

		xor_flags |= ASYNC_TX_XOR_DROP_DST;
		xor_flags &= ~ASYNC_TX_ACK;

		tx = async_xor(dest, src_list, offset, src_cnt, len, xor_flags,
			depend_tx, NULL, NULL);

		async_tx_quiesce(&tx);

		*result = page_is_zero(dest, offset, len) ? 0 : 1;

		async_tx_sync_epilog(cb_fn, cb_param);
	}

	return tx;
}
EXPORT_SYMBOL_GPL(async_xor_zero_sum);

static int __init async_xor_init(void)
{
	#ifdef CONFIG_DMA_ENGINE
	/* To conserve stack space the input src_list (array of page pointers)
	 * is reused to hold the array of dma addresses passed to the driver.
	 * This conversion is only possible when dma_addr_t is less than the
	 * the size of a pointer.  HIGHMEM64G is known to violate this
	 * assumption.
	 */
	BUILD_BUG_ON(sizeof(dma_addr_t) > sizeof(struct page *));
	#endif

	return 0;
}

static void __exit async_xor_exit(void)
{
	do { } while (0);
}

module_init(async_xor_init);
module_exit(async_xor_exit);

MODULE_AUTHOR("Intel Corporation");
MODULE_DESCRIPTION("asynchronous xor/xor-zero-sum api");
MODULE_LICENSE("GPL");
