

#include <linux/etherdevice.h>
#include <net/mac80211.h>
#include "iwl-eeprom.h"
#include "iwl-dev.h"
#include "iwl-core.h"
#include "iwl-sta.h"
#include "iwl-io.h"
#include "iwl-helpers.h"

static const u16 default_tid_to_tx_fifo[] = {
	IWL_TX_FIFO_AC1,
	IWL_TX_FIFO_AC0,
	IWL_TX_FIFO_AC0,
	IWL_TX_FIFO_AC1,
	IWL_TX_FIFO_AC2,
	IWL_TX_FIFO_AC2,
	IWL_TX_FIFO_AC3,
	IWL_TX_FIFO_AC3,
	IWL_TX_FIFO_NONE,
	IWL_TX_FIFO_NONE,
	IWL_TX_FIFO_NONE,
	IWL_TX_FIFO_NONE,
	IWL_TX_FIFO_NONE,
	IWL_TX_FIFO_NONE,
	IWL_TX_FIFO_NONE,
	IWL_TX_FIFO_NONE,
	IWL_TX_FIFO_AC3
};

static inline int iwl_alloc_dma_ptr(struct iwl_priv *priv,
				    struct iwl_dma_ptr *ptr, size_t size)
{
	ptr->addr = pci_alloc_consistent(priv->pci_dev, size, &ptr->dma);
	if (!ptr->addr)
		return -ENOMEM;
	ptr->size = size;
	return 0;
}

static inline void iwl_free_dma_ptr(struct iwl_priv *priv,
				    struct iwl_dma_ptr *ptr)
{
	if (unlikely(!ptr->addr))
		return;

	pci_free_consistent(priv->pci_dev, ptr->size, ptr->addr, ptr->dma);
	memset(ptr, 0, sizeof(*ptr));
}

static inline dma_addr_t iwl_tfd_tb_get_addr(struct iwl_tfd *tfd, u8 idx)
{
	struct iwl_tfd_tb *tb = &tfd->tbs[idx];

	dma_addr_t addr = get_unaligned_le32(&tb->lo);
	if (sizeof(dma_addr_t) > sizeof(u32))
		addr |=
		((dma_addr_t)(le16_to_cpu(tb->hi_n_len) & 0xF) << 16) << 16;

	return addr;
}

static inline u16 iwl_tfd_tb_get_len(struct iwl_tfd *tfd, u8 idx)
{
	struct iwl_tfd_tb *tb = &tfd->tbs[idx];

	return le16_to_cpu(tb->hi_n_len) >> 4;
}

static inline void iwl_tfd_set_tb(struct iwl_tfd *tfd, u8 idx,
				  dma_addr_t addr, u16 len)
{
	struct iwl_tfd_tb *tb = &tfd->tbs[idx];
	u16 hi_n_len = len << 4;

	put_unaligned_le32(addr, &tb->lo);
	if (sizeof(dma_addr_t) > sizeof(u32))
		hi_n_len |= ((addr >> 16) >> 16) & 0xF;

	tb->hi_n_len = cpu_to_le16(hi_n_len);

	tfd->num_tbs = idx + 1;
}

static inline u8 iwl_tfd_get_num_tbs(struct iwl_tfd *tfd)
{
	return tfd->num_tbs & 0x1f;
}

static void iwl_hw_txq_free_tfd(struct iwl_priv *priv, struct iwl_tx_queue *txq)
{
	struct iwl_tfd *tfd_tmp = (struct iwl_tfd *)&txq->tfds[0];
	struct iwl_tfd *tfd;
	struct pci_dev *dev = priv->pci_dev;
	int index = txq->q.read_ptr;
	int i;
	int num_tbs;

	tfd = &tfd_tmp[index];

	/* Sanity check on number of chunks */
	num_tbs = iwl_tfd_get_num_tbs(tfd);

	if (num_tbs >= IWL_NUM_OF_TBS) {
		IWL_ERROR("Too many chunks: %i\n", num_tbs);
		/* @todo issue fatal error, it is quite serious situation */
		return;
	}

	/* Unmap tx_cmd */
	if (num_tbs)
		pci_unmap_single(dev,
				pci_unmap_addr(&txq->cmd[index]->meta, mapping),
				pci_unmap_len(&txq->cmd[index]->meta, len),
				PCI_DMA_BIDIRECTIONAL);

	/* Unmap chunks, if any. */
	for (i = 1; i < num_tbs; i++) {
		pci_unmap_single(dev, iwl_tfd_tb_get_addr(tfd, i),
				iwl_tfd_tb_get_len(tfd, i), PCI_DMA_TODEVICE);

		if (txq->txb) {
			dev_kfree_skb(txq->txb[txq->q.read_ptr].skb[i - 1]);
			txq->txb[txq->q.read_ptr].skb[i - 1] = NULL;
		}
	}
}

static int iwl_hw_txq_attach_buf_to_tfd(struct iwl_priv *priv,
					struct iwl_tfd *tfd,
					dma_addr_t addr, u16 len)
{

	u32 num_tbs = iwl_tfd_get_num_tbs(tfd);

	/* Each TFD can point to a maximum 20 Tx buffers */
	if (num_tbs >= IWL_NUM_OF_TBS) {
		IWL_ERROR("Error can not send more than %d chunks\n",
			  IWL_NUM_OF_TBS);
		return -EINVAL;
	}

	BUG_ON(addr & ~DMA_BIT_MASK(36));
	if (unlikely(addr & ~IWL_TX_DMA_MASK))
		IWL_ERROR("Unaligned address = %llx\n",
			  (unsigned long long)addr);

	iwl_tfd_set_tb(tfd, num_tbs, addr, len);

	return 0;
}

int iwl_txq_update_write_ptr(struct iwl_priv *priv, struct iwl_tx_queue *txq)
{
	u32 reg = 0;
	int ret = 0;
	int txq_id = txq->q.id;

	if (txq->need_update == 0)
		return ret;

	/* if we're trying to save power */
	if (test_bit(STATUS_POWER_PMI, &priv->status)) {
		/* wake up nic if it's powered down ...
		 * uCode will wake up, and interrupt us again, so next
		 * time we'll skip this part. */
		reg = iwl_read32(priv, CSR_UCODE_DRV_GP1);

		if (reg & CSR_UCODE_DRV_GP1_BIT_MAC_SLEEP) {
			IWL_DEBUG_INFO("Requesting wakeup, GP1 = 0x%x\n", reg);
			iwl_set_bit(priv, CSR_GP_CNTRL,
				    CSR_GP_CNTRL_REG_FLAG_MAC_ACCESS_REQ);
			return ret;
		}

		/* restore this queue's parameters in nic hardware. */
		ret = iwl_grab_nic_access(priv);
		if (ret)
			return ret;
		iwl_write_direct32(priv, HBUS_TARG_WRPTR,
				     txq->q.write_ptr | (txq_id << 8));
		iwl_release_nic_access(priv);

	/* else not in power-save mode, uCode will never sleep when we're
	 * trying to tx (during RFKILL, we're not trying to tx). */
	} else
		iwl_write32(priv, HBUS_TARG_WRPTR,
			    txq->q.write_ptr | (txq_id << 8));

	txq->need_update = 0;

	return ret;
}
EXPORT_SYMBOL(iwl_txq_update_write_ptr);


static void iwl_tx_queue_free(struct iwl_priv *priv, int txq_id)
{
	struct iwl_tx_queue *txq = &priv->txq[txq_id];
	struct iwl_queue *q = &txq->q;
	struct pci_dev *dev = priv->pci_dev;
	int i, len;

	if (q->n_bd == 0)
		return;

	/* first, empty all BD's */
	for (; q->write_ptr != q->read_ptr;
	     q->read_ptr = iwl_queue_inc_wrap(q->read_ptr, q->n_bd))
		iwl_hw_txq_free_tfd(priv, txq);

	len = sizeof(struct iwl_cmd) * q->n_window;

	/* De-alloc array of command/tx buffers */
	for (i = 0; i < TFD_TX_CMD_SLOTS; i++)
		kfree(txq->cmd[i]);

	/* De-alloc circular buffer of TFDs */
	if (txq->q.n_bd)
		pci_free_consistent(dev, sizeof(struct iwl_tfd) *
				    txq->q.n_bd, txq->tfds, txq->q.dma_addr);

	/* De-alloc array of per-TFD driver data */
	kfree(txq->txb);
	txq->txb = NULL;

	/* 0-fill queue descriptor structure */
	memset(txq, 0, sizeof(*txq));
}


static void iwl_cmd_queue_free(struct iwl_priv *priv)
{
	struct iwl_tx_queue *txq = &priv->txq[IWL_CMD_QUEUE_NUM];
	struct iwl_queue *q = &txq->q;
	struct pci_dev *dev = priv->pci_dev;
	int i, len;

	if (q->n_bd == 0)
		return;

	len = sizeof(struct iwl_cmd) * q->n_window;
	len += IWL_MAX_SCAN_SIZE;

	/* De-alloc array of command/tx buffers */
	for (i = 0; i <= TFD_CMD_SLOTS; i++)
		kfree(txq->cmd[i]);

	/* De-alloc circular buffer of TFDs */
	if (txq->q.n_bd)
		pci_free_consistent(dev, sizeof(struct iwl_tfd) *
				    txq->q.n_bd, txq->tfds, txq->q.dma_addr);

	/* 0-fill queue descriptor structure */
	memset(txq, 0, sizeof(*txq));
}

int iwl_queue_space(const struct iwl_queue *q)
{
	int s = q->read_ptr - q->write_ptr;

	if (q->read_ptr > q->write_ptr)
		s -= q->n_bd;

	if (s <= 0)
		s += q->n_window;
	/* keep some reserve to not confuse empty and full situations */
	s -= 2;
	if (s < 0)
		s = 0;
	return s;
}
EXPORT_SYMBOL(iwl_queue_space);


static int iwl_queue_init(struct iwl_priv *priv, struct iwl_queue *q,
			  int count, int slots_num, u32 id)
{
	q->n_bd = count;
	q->n_window = slots_num;
	q->id = id;

	/* count must be power-of-two size, otherwise iwl_queue_inc_wrap
	 * and iwl_queue_dec_wrap are broken. */
	BUG_ON(!is_power_of_2(count));

	/* slots_num must be power-of-two size, otherwise
	 * get_cmd_index is broken. */
	BUG_ON(!is_power_of_2(slots_num));

	q->low_mark = q->n_window / 4;
	if (q->low_mark < 4)
		q->low_mark = 4;

	q->high_mark = q->n_window / 8;
	if (q->high_mark < 2)
		q->high_mark = 2;

	q->write_ptr = q->read_ptr = 0;

	return 0;
}

static int iwl_tx_queue_alloc(struct iwl_priv *priv,
			      struct iwl_tx_queue *txq, u32 id)
{
	struct pci_dev *dev = priv->pci_dev;

	/* Driver private data, only for Tx (not command) queues,
	 * not shared with device. */
	if (id != IWL_CMD_QUEUE_NUM) {
		txq->txb = kmalloc(sizeof(txq->txb[0]) *
				   TFD_QUEUE_SIZE_MAX, GFP_KERNEL);
		if (!txq->txb) {
			IWL_ERROR("kmalloc for auxiliary BD "
				  "structures failed\n");
			goto error;
		}
	} else
		txq->txb = NULL;

	/* Circular buffer of transmit frame descriptors (TFDs),
	 * shared with device */
	txq->tfds = pci_alloc_consistent(dev,
			sizeof(txq->tfds[0]) * TFD_QUEUE_SIZE_MAX,
			&txq->q.dma_addr);

	if (!txq->tfds) {
		IWL_ERROR("pci_alloc_consistent(%zd) failed\n",
			  sizeof(txq->tfds[0]) * TFD_QUEUE_SIZE_MAX);
		goto error;
	}
	txq->q.id = id;

	return 0;

 error:
	kfree(txq->txb);
	txq->txb = NULL;

	return -ENOMEM;
}

static int iwl_hw_tx_queue_init(struct iwl_priv *priv,
				struct iwl_tx_queue *txq)
{
	int ret;
	unsigned long flags;
	int txq_id = txq->q.id;

	spin_lock_irqsave(&priv->lock, flags);
	ret = iwl_grab_nic_access(priv);
	if (ret) {
		spin_unlock_irqrestore(&priv->lock, flags);
		return ret;
	}

	/* Circular buffer (TFD queue in DRAM) physical base address */
	iwl_write_direct32(priv, FH_MEM_CBBC_QUEUE(txq_id),
			     txq->q.dma_addr >> 8);

	iwl_release_nic_access(priv);
	spin_unlock_irqrestore(&priv->lock, flags);

	return 0;
}

static int iwl_tx_queue_init(struct iwl_priv *priv, struct iwl_tx_queue *txq,
			     int slots_num, u32 txq_id)
{
	int i, len;
	int ret;

	/*
	 * Alloc buffer array for commands (Tx or other types of commands).
	 * For the command queue (#4), allocate command space + one big
	 * command for scan, since scan command is very huge; the system will
	 * not have two scans at the same time, so only one is needed.
	 * For normal Tx queues (all other queues), no super-size command
	 * space is needed.
	 */
	len = sizeof(struct iwl_cmd);
	for (i = 0; i <= slots_num; i++) {
		if (i == slots_num) {
			if (txq_id == IWL_CMD_QUEUE_NUM)
				len += IWL_MAX_SCAN_SIZE;
			else
				continue;
		}

		txq->cmd[i] = kmalloc(len, GFP_KERNEL);
		if (!txq->cmd[i])
			goto err;
	}

	/* Alloc driver data array and TFD circular buffer */
	ret = iwl_tx_queue_alloc(priv, txq, txq_id);
	if (ret)
		goto err;

	txq->need_update = 0;

	/* TFD_QUEUE_SIZE_MAX must be power-of-two size, otherwise
	 * iwl_queue_inc_wrap and iwl_queue_dec_wrap are broken. */
	BUILD_BUG_ON(TFD_QUEUE_SIZE_MAX & (TFD_QUEUE_SIZE_MAX - 1));

	/* Initialize queue's high/low-water marks, and head/tail indexes */
	iwl_queue_init(priv, &txq->q, TFD_QUEUE_SIZE_MAX, slots_num, txq_id);

	/* Tell device where to find queue */
	iwl_hw_tx_queue_init(priv, txq);

	return 0;
err:
	for (i = 0; i < slots_num; i++) {
		kfree(txq->cmd[i]);
		txq->cmd[i] = NULL;
	}

	if (txq_id == IWL_CMD_QUEUE_NUM) {
		kfree(txq->cmd[slots_num]);
		txq->cmd[slots_num] = NULL;
	}
	return -ENOMEM;
}
void iwl_hw_txq_ctx_free(struct iwl_priv *priv)
{
	int txq_id;

	/* Tx queues */
	for (txq_id = 0; txq_id < priv->hw_params.max_txq_num; txq_id++)
		if (txq_id == IWL_CMD_QUEUE_NUM)
			iwl_cmd_queue_free(priv);
		else
			iwl_tx_queue_free(priv, txq_id);

	iwl_free_dma_ptr(priv, &priv->kw);

	iwl_free_dma_ptr(priv, &priv->scd_bc_tbls);
}
EXPORT_SYMBOL(iwl_hw_txq_ctx_free);

int iwl_txq_ctx_reset(struct iwl_priv *priv)
{
	int ret = 0;
	int txq_id, slots_num;
	unsigned long flags;

	/* Free all tx/cmd queues and keep-warm buffer */
	iwl_hw_txq_ctx_free(priv);

	ret = iwl_alloc_dma_ptr(priv, &priv->scd_bc_tbls,
				priv->hw_params.scd_bc_tbls_size);
	if (ret) {
		IWL_ERROR("Scheduler BC Table allocation failed\n");
		goto error_bc_tbls;
	}
	/* Alloc keep-warm buffer */
	ret = iwl_alloc_dma_ptr(priv, &priv->kw, IWL_KW_SIZE);
	if (ret) {
		IWL_ERROR("Keep Warm allocation failed\n");
		goto error_kw;
	}
	spin_lock_irqsave(&priv->lock, flags);
	ret = iwl_grab_nic_access(priv);
	if (unlikely(ret)) {
		spin_unlock_irqrestore(&priv->lock, flags);
		goto error_reset;
	}

	/* Turn off all Tx DMA fifos */
	priv->cfg->ops->lib->txq_set_sched(priv, 0);

	/* Tell NIC where to find the "keep warm" buffer */
	iwl_write_direct32(priv, FH_KW_MEM_ADDR_REG, priv->kw.dma >> 4);

	iwl_release_nic_access(priv);
	spin_unlock_irqrestore(&priv->lock, flags);

	/* Alloc and init all Tx queues, including the command queue (#4) */
	for (txq_id = 0; txq_id < priv->hw_params.max_txq_num; txq_id++) {
		slots_num = (txq_id == IWL_CMD_QUEUE_NUM) ?
					TFD_CMD_SLOTS : TFD_TX_CMD_SLOTS;
		ret = iwl_tx_queue_init(priv, &priv->txq[txq_id], slots_num,
				       txq_id);
		if (ret) {
			IWL_ERROR("Tx %d queue init failed\n", txq_id);
			goto error;
		}
	}

	return ret;

 error:
	iwl_hw_txq_ctx_free(priv);
 error_reset:
	iwl_free_dma_ptr(priv, &priv->kw);
 error_kw:
	iwl_free_dma_ptr(priv, &priv->scd_bc_tbls);
 error_bc_tbls:
	return ret;
}

void iwl_txq_ctx_stop(struct iwl_priv *priv)
{
	int ch;
	unsigned long flags;

	/* Turn off all Tx DMA fifos */
	spin_lock_irqsave(&priv->lock, flags);
	if (iwl_grab_nic_access(priv)) {
		spin_unlock_irqrestore(&priv->lock, flags);
		return;
	}

	priv->cfg->ops->lib->txq_set_sched(priv, 0);

	/* Stop each Tx DMA channel, and wait for it to be idle */
	for (ch = 0; ch < priv->hw_params.dma_chnl_num; ch++) {
		iwl_write_direct32(priv, FH_TCSR_CHNL_TX_CONFIG_REG(ch), 0x0);
		iwl_poll_direct_bit(priv, FH_TSSR_TX_STATUS_REG,
				    FH_TSSR_TX_STATUS_REG_MSK_CHNL_IDLE(ch),
				    1000);
	}
	iwl_release_nic_access(priv);
	spin_unlock_irqrestore(&priv->lock, flags);

	/* Deallocate memory for all Tx queues */
	iwl_hw_txq_ctx_free(priv);
}
EXPORT_SYMBOL(iwl_txq_ctx_stop);

static void iwl_tx_cmd_build_basic(struct iwl_priv *priv,
				  struct iwl_tx_cmd *tx_cmd,
				  struct ieee80211_tx_info *info,
				  struct ieee80211_hdr *hdr,
				  u8 std_id)
{
	__le16 fc = hdr->frame_control;
	__le32 tx_flags = tx_cmd->tx_flags;

	tx_cmd->stop_time.life_time = TX_CMD_LIFE_TIME_INFINITE;
	if (!(info->flags & IEEE80211_TX_CTL_NO_ACK)) {
		tx_flags |= TX_CMD_FLG_ACK_MSK;
		if (ieee80211_is_mgmt(fc))
			tx_flags |= TX_CMD_FLG_SEQ_CTL_MSK;
		if (ieee80211_is_probe_resp(fc) &&
		    !(le16_to_cpu(hdr->seq_ctrl) & 0xf))
			tx_flags |= TX_CMD_FLG_TSF_MSK;
	} else {
		tx_flags &= (~TX_CMD_FLG_ACK_MSK);
		tx_flags |= TX_CMD_FLG_SEQ_CTL_MSK;
	}

	if (ieee80211_is_back_req(fc))
		tx_flags |= TX_CMD_FLG_ACK_MSK | TX_CMD_FLG_IMM_BA_RSP_MASK;


	tx_cmd->sta_id = std_id;
	if (ieee80211_has_morefrags(fc))
		tx_flags |= TX_CMD_FLG_MORE_FRAG_MSK;

	if (ieee80211_is_data_qos(fc)) {
		u8 *qc = ieee80211_get_qos_ctl(hdr);
		tx_cmd->tid_tspec = qc[0] & 0xf;
		tx_flags &= ~TX_CMD_FLG_SEQ_CTL_MSK;
	} else {
		tx_flags |= TX_CMD_FLG_SEQ_CTL_MSK;
	}

	priv->cfg->ops->utils->rts_tx_cmd_flag(info, &tx_flags);

	if ((tx_flags & TX_CMD_FLG_RTS_MSK) || (tx_flags & TX_CMD_FLG_CTS_MSK))
		tx_flags |= TX_CMD_FLG_FULL_TXOP_PROT_MSK;

	tx_flags &= ~(TX_CMD_FLG_ANT_SEL_MSK);
	if (ieee80211_is_mgmt(fc)) {
		if (ieee80211_is_assoc_req(fc) || ieee80211_is_reassoc_req(fc))
			tx_cmd->timeout.pm_frame_timeout = cpu_to_le16(3);
		else
			tx_cmd->timeout.pm_frame_timeout = cpu_to_le16(2);
	} else {
		tx_cmd->timeout.pm_frame_timeout = 0;
	}

	tx_cmd->driver_txop = 0;
	tx_cmd->tx_flags = tx_flags;
	tx_cmd->next_frame_len = 0;
}

#define RTS_HCCA_RETRY_LIMIT		3
#define RTS_DFAULT_RETRY_LIMIT		60

static void iwl_tx_cmd_build_rate(struct iwl_priv *priv,
			      struct iwl_tx_cmd *tx_cmd,
			      struct ieee80211_tx_info *info,
			      __le16 fc, int sta_id,
			      int is_hcca)
{
	u32 rate_flags = 0;
	int rate_idx;
	u8 rts_retry_limit = 0;
	u8 data_retry_limit = 0;
	u8 rate_plcp;

	rate_idx = min(ieee80211_get_tx_rate(priv->hw, info)->hw_value & 0xffff,
			IWL_RATE_COUNT - 1);

	rate_plcp = iwl_rates[rate_idx].plcp;

	rts_retry_limit = (is_hcca) ?
	    RTS_HCCA_RETRY_LIMIT : RTS_DFAULT_RETRY_LIMIT;

	if ((rate_idx >= IWL_FIRST_CCK_RATE) && (rate_idx <= IWL_LAST_CCK_RATE))
		rate_flags |= RATE_MCS_CCK_MSK;


	if (ieee80211_is_probe_resp(fc)) {
		data_retry_limit = 3;
		if (data_retry_limit < rts_retry_limit)
			rts_retry_limit = data_retry_limit;
	} else
		data_retry_limit = IWL_DEFAULT_TX_RETRY;

	if (priv->data_retry_limit != -1)
		data_retry_limit = priv->data_retry_limit;


	if (ieee80211_is_data(fc)) {
		tx_cmd->initial_rate_index = 0;
		tx_cmd->tx_flags |= TX_CMD_FLG_STA_RATE_MSK;
	} else {
		switch (fc & cpu_to_le16(IEEE80211_FCTL_STYPE)) {
		case cpu_to_le16(IEEE80211_STYPE_AUTH):
		case cpu_to_le16(IEEE80211_STYPE_DEAUTH):
		case cpu_to_le16(IEEE80211_STYPE_ASSOC_REQ):
		case cpu_to_le16(IEEE80211_STYPE_REASSOC_REQ):
			if (tx_cmd->tx_flags & TX_CMD_FLG_RTS_MSK) {
				tx_cmd->tx_flags &= ~TX_CMD_FLG_RTS_MSK;
				tx_cmd->tx_flags |= TX_CMD_FLG_CTS_MSK;
			}
			break;
		default:
			break;
		}

		priv->mgmt_tx_ant = iwl_toggle_tx_ant(priv, priv->mgmt_tx_ant);
		rate_flags |= iwl_ant_idx_to_flags(priv->mgmt_tx_ant);
	}

	tx_cmd->rts_retry_limit = rts_retry_limit;
	tx_cmd->data_retry_limit = data_retry_limit;
	tx_cmd->rate_n_flags = iwl_hw_set_rate_n_flags(rate_plcp, rate_flags);
}

static void iwl_tx_cmd_build_hwcrypto(struct iwl_priv *priv,
				      struct ieee80211_tx_info *info,
				      struct iwl_tx_cmd *tx_cmd,
				      struct sk_buff *skb_frag,
				      int sta_id)
{
	struct ieee80211_key_conf *keyconf = info->control.hw_key;

	switch (keyconf->alg) {
	case ALG_CCMP:
		tx_cmd->sec_ctl = TX_CMD_SEC_CCM;
		memcpy(tx_cmd->key, keyconf->key, keyconf->keylen);
		if (info->flags & IEEE80211_TX_CTL_AMPDU)
			tx_cmd->tx_flags |= TX_CMD_FLG_AGG_CCMP_MSK;
		IWL_DEBUG_TX("tx_cmd with AES hwcrypto\n");
		break;

	case ALG_TKIP:
		tx_cmd->sec_ctl = TX_CMD_SEC_TKIP;
		ieee80211_get_tkip_key(keyconf, skb_frag,
			IEEE80211_TKIP_P2_KEY, tx_cmd->key);
		IWL_DEBUG_TX("tx_cmd with tkip hwcrypto\n");
		break;

	case ALG_WEP:
		tx_cmd->sec_ctl |= (TX_CMD_SEC_WEP |
			(keyconf->keyidx & TX_CMD_SEC_MSK) << TX_CMD_SEC_SHIFT);

		if (keyconf->keylen == WEP_KEY_LEN_128)
			tx_cmd->sec_ctl |= TX_CMD_SEC_KEY128;

		memcpy(&tx_cmd->key[3], keyconf->key, keyconf->keylen);

		IWL_DEBUG_TX("Configuring packet for WEP encryption "
			     "with key %d\n", keyconf->keyidx);
		break;

	default:
		printk(KERN_ERR "Unknown encode alg %d\n", keyconf->alg);
		break;
	}
}

static void iwl_update_tx_stats(struct iwl_priv *priv, u16 fc, u16 len)
{
	/* 0 - mgmt, 1 - cnt, 2 - data */
	int idx = (fc & IEEE80211_FCTL_FTYPE) >> 2;
	priv->tx_stats[idx].cnt++;
	priv->tx_stats[idx].bytes += len;
}

int iwl_tx_skb(struct iwl_priv *priv, struct sk_buff *skb)
{
	struct ieee80211_hdr *hdr = (struct ieee80211_hdr *)skb->data;
	struct ieee80211_tx_info *info = IEEE80211_SKB_CB(skb);
	struct iwl_tfd *tfd;
	struct iwl_tx_queue *txq;
	struct iwl_queue *q;
	struct iwl_cmd *out_cmd;
	struct iwl_tx_cmd *tx_cmd;
	int swq_id, txq_id;
	dma_addr_t phys_addr;
	dma_addr_t txcmd_phys;
	dma_addr_t scratch_phys;
	u16 len, len_org;
	u16 seq_number = 0;
	__le16 fc;
	u8 hdr_len;
	u8 sta_id;
	u8 wait_write_ptr = 0;
	u8 tid = 0;
	u8 *qc = NULL;
	unsigned long flags;
	int ret;

	spin_lock_irqsave(&priv->lock, flags);
	if (iwl_is_rfkill(priv)) {
		IWL_DEBUG_DROP("Dropping - RF KILL\n");
		goto drop_unlock;
	}

	if ((ieee80211_get_tx_rate(priv->hw, info)->hw_value & 0xFF) ==
	     IWL_INVALID_RATE) {
		IWL_ERROR("ERROR: No TX rate available.\n");
		goto drop_unlock;
	}

	fc = hdr->frame_control;

#ifdef CONFIG_IWLWIFI_DEBUG
	if (ieee80211_is_auth(fc))
		IWL_DEBUG_TX("Sending AUTH frame\n");
	else if (ieee80211_is_assoc_req(fc))
		IWL_DEBUG_TX("Sending ASSOC frame\n");
	else if (ieee80211_is_reassoc_req(fc))
		IWL_DEBUG_TX("Sending REASSOC frame\n");
#endif

	/* drop all data frame if we are not associated */
	if (ieee80211_is_data(fc) &&
	    (priv->iw_mode != NL80211_IFTYPE_MONITOR ||
	    !(info->flags & IEEE80211_TX_CTL_INJECTED)) && /* packet injection */
	    (!iwl_is_associated(priv) ||
	     ((priv->iw_mode == NL80211_IFTYPE_STATION) && !priv->assoc_id) ||
	     !priv->assoc_station_added)) {
		IWL_DEBUG_DROP("Dropping - !iwl_is_associated\n");
		goto drop_unlock;
	}

	spin_unlock_irqrestore(&priv->lock, flags);

	hdr_len = ieee80211_hdrlen(fc);

	/* Find (or create) index into station table for destination station */
	sta_id = iwl_get_sta_id(priv, hdr);
	if (sta_id == IWL_INVALID_STATION) {
		IWL_DEBUG_DROP("Dropping - INVALID STATION: %pM\n",
			       hdr->addr1);
		goto drop;
	}

	IWL_DEBUG_TX("station Id %d\n", sta_id);

	swq_id = skb_get_queue_mapping(skb);
	txq_id = swq_id;
	if (ieee80211_is_data_qos(fc)) {
		qc = ieee80211_get_qos_ctl(hdr);
		tid = qc[0] & IEEE80211_QOS_CTL_TID_MASK;
		seq_number = priv->stations[sta_id].tid[tid].seq_number;
		seq_number &= IEEE80211_SCTL_SEQ;
		hdr->seq_ctrl = hdr->seq_ctrl &
				__constant_cpu_to_le16(IEEE80211_SCTL_FRAG);
		hdr->seq_ctrl |= cpu_to_le16(seq_number);
		seq_number += 0x10;
		/* aggregation is on for this <sta,tid> */
		if (info->flags & IEEE80211_TX_CTL_AMPDU)
			txq_id = priv->stations[sta_id].tid[tid].agg.txq_id;
		priv->stations[sta_id].tid[tid].tfds_in_queue++;
	}

	txq = &priv->txq[txq_id];
	q = &txq->q;
	txq->swq_id = swq_id;

	spin_lock_irqsave(&priv->lock, flags);

	/* Set up first empty TFD within this queue's circular TFD buffer */
	tfd = &txq->tfds[q->write_ptr];
	memset(tfd, 0, sizeof(*tfd));

	/* Set up driver data for this TFD */
	memset(&(txq->txb[q->write_ptr]), 0, sizeof(struct iwl_tx_info));
	txq->txb[q->write_ptr].skb[0] = skb;

	/* Set up first empty entry in queue's array of Tx/cmd buffers */
	out_cmd = txq->cmd[q->write_ptr];
	tx_cmd = &out_cmd->cmd.tx;
	memset(&out_cmd->hdr, 0, sizeof(out_cmd->hdr));
	memset(tx_cmd, 0, sizeof(struct iwl_tx_cmd));

	/*
	 * Set up the Tx-command (not MAC!) header.
	 * Store the chosen Tx queue and TFD index within the sequence field;
	 * after Tx, uCode's Tx response will return this value so driver can
	 * locate the frame within the tx queue and do post-tx processing.
	 */
	out_cmd->hdr.cmd = REPLY_TX;
	out_cmd->hdr.sequence = cpu_to_le16((u16)(QUEUE_TO_SEQ(txq_id) |
				INDEX_TO_SEQ(q->write_ptr)));

	/* Copy MAC header from skb into command buffer */
	memcpy(tx_cmd->hdr, hdr, hdr_len);

	/*
	 * Use the first empty entry in this queue's command buffer array
	 * to contain the Tx command and MAC header concatenated together
	 * (payload data will be in another buffer).
	 * Size of this varies, due to varying MAC header length.
	 * If end is not dword aligned, we'll have 2 extra bytes at the end
	 * of the MAC header (device reads on dword boundaries).
	 * We'll tell device about this padding later.
	 */
	len = sizeof(struct iwl_tx_cmd) +
		sizeof(struct iwl_cmd_header) + hdr_len;

	len_org = len;
	len = (len + 3) & ~3;

	if (len_org != len)
		len_org = 1;
	else
		len_org = 0;

	/* Physical address of this Tx command's header (not MAC header!),
	 * within command buffer array. */
	txcmd_phys = pci_map_single(priv->pci_dev,
				    out_cmd, sizeof(struct iwl_cmd),
				    PCI_DMA_BIDIRECTIONAL);
	pci_unmap_addr_set(&out_cmd->meta, mapping, txcmd_phys);
	pci_unmap_len_set(&out_cmd->meta, len, sizeof(struct iwl_cmd));
	/* Add buffer containing Tx command and MAC(!) header to TFD's
	 * first entry */
	txcmd_phys += offsetof(struct iwl_cmd, hdr);
	iwl_hw_txq_attach_buf_to_tfd(priv, tfd, txcmd_phys, len);

	if (info->control.hw_key)
		iwl_tx_cmd_build_hwcrypto(priv, info, tx_cmd, skb, sta_id);

	/* Set up TFD's 2nd entry to point directly to remainder of skb,
	 * if any (802.11 null frames have no payload). */
	len = skb->len - hdr_len;
	if (len) {
		phys_addr = pci_map_single(priv->pci_dev, skb->data + hdr_len,
					   len, PCI_DMA_TODEVICE);
		iwl_hw_txq_attach_buf_to_tfd(priv, tfd, phys_addr, len);
	}

	/* Tell NIC about any 2-byte padding after MAC header */
	if (len_org)
		tx_cmd->tx_flags |= TX_CMD_FLG_MH_PAD_MSK;

	/* Total # bytes to be transmitted */
	len = (u16)skb->len;
	tx_cmd->len = cpu_to_le16(len);
	/* TODO need this for burst mode later on */
	iwl_tx_cmd_build_basic(priv, tx_cmd, info, hdr, sta_id);

	/* set is_hcca to 0; it probably will never be implemented */
	iwl_tx_cmd_build_rate(priv, tx_cmd, info, fc, sta_id, 0);

	iwl_update_tx_stats(priv, le16_to_cpu(fc), len);

	scratch_phys = txcmd_phys + sizeof(struct iwl_cmd_header) +
		offsetof(struct iwl_tx_cmd, scratch);
	tx_cmd->dram_lsb_ptr = cpu_to_le32(scratch_phys);
	tx_cmd->dram_msb_ptr = iwl_get_dma_hi_addr(scratch_phys);

	if (!ieee80211_has_morefrags(hdr->frame_control)) {
		txq->need_update = 1;
		if (qc)
			priv->stations[sta_id].tid[tid].seq_number = seq_number;
	} else {
		wait_write_ptr = 1;
		txq->need_update = 0;
	}

	iwl_print_hex_dump(priv, IWL_DL_TX, (u8 *)tx_cmd, sizeof(*tx_cmd));

	iwl_print_hex_dump(priv, IWL_DL_TX, (u8 *)tx_cmd->hdr, hdr_len);

	/* Set up entry for this TFD in Tx byte-count array */
	priv->cfg->ops->lib->txq_update_byte_cnt_tbl(priv, txq, len);

	/* Tell device the write index *just past* this latest filled TFD */
	q->write_ptr = iwl_queue_inc_wrap(q->write_ptr, q->n_bd);
	ret = iwl_txq_update_write_ptr(priv, txq);
	spin_unlock_irqrestore(&priv->lock, flags);

	if (ret)
		return ret;

	if ((iwl_queue_space(q) < q->high_mark) && priv->mac80211_registered) {
		if (wait_write_ptr) {
			spin_lock_irqsave(&priv->lock, flags);
			txq->need_update = 1;
			iwl_txq_update_write_ptr(priv, txq);
			spin_unlock_irqrestore(&priv->lock, flags);
		} else {
			ieee80211_stop_queue(priv->hw, txq->swq_id);
		}
	}

	return 0;

drop_unlock:
	spin_unlock_irqrestore(&priv->lock, flags);
drop:
	return -1;
}
EXPORT_SYMBOL(iwl_tx_skb);

/*************** HOST COMMAND QUEUE FUNCTIONS   *****/

int iwl_enqueue_hcmd(struct iwl_priv *priv, struct iwl_host_cmd *cmd)
{
	struct iwl_tx_queue *txq = &priv->txq[IWL_CMD_QUEUE_NUM];
	struct iwl_queue *q = &txq->q;
	struct iwl_tfd *tfd;
	struct iwl_cmd *out_cmd;
	dma_addr_t phys_addr;
	unsigned long flags;
	int len, ret;
	u32 idx;
	u16 fix_size;

	cmd->len = priv->cfg->ops->utils->get_hcmd_size(cmd->id, cmd->len);
	fix_size = (u16)(cmd->len + sizeof(out_cmd->hdr));

	/* If any of the command structures end up being larger than
	 * the TFD_MAX_PAYLOAD_SIZE, and it sent as a 'small' command then
	 * we will need to increase the size of the TFD entries */
	BUG_ON((fix_size > TFD_MAX_PAYLOAD_SIZE) &&
	       !(cmd->meta.flags & CMD_SIZE_HUGE));

	if (iwl_is_rfkill(priv)) {
		IWL_DEBUG_INFO("Not sending command - RF KILL");
		return -EIO;
	}

	if (iwl_queue_space(q) < ((cmd->meta.flags & CMD_ASYNC) ? 2 : 1)) {
		IWL_ERROR("No space for Tx\n");
		return -ENOSPC;
	}

	spin_lock_irqsave(&priv->hcmd_lock, flags);

	tfd = &txq->tfds[q->write_ptr];
	memset(tfd, 0, sizeof(*tfd));


	idx = get_cmd_index(q, q->write_ptr, cmd->meta.flags & CMD_SIZE_HUGE);
	out_cmd = txq->cmd[idx];

	out_cmd->hdr.cmd = cmd->id;
	memcpy(&out_cmd->meta, &cmd->meta, sizeof(cmd->meta));
	memcpy(&out_cmd->cmd.payload, cmd->data, cmd->len);

	/* At this point, the out_cmd now has all of the incoming cmd
	 * information */

	out_cmd->hdr.flags = 0;
	out_cmd->hdr.sequence = cpu_to_le16(QUEUE_TO_SEQ(IWL_CMD_QUEUE_NUM) |
			INDEX_TO_SEQ(q->write_ptr));
	if (out_cmd->meta.flags & CMD_SIZE_HUGE)
		out_cmd->hdr.sequence |= SEQ_HUGE_FRAME;
	len = (idx == TFD_CMD_SLOTS) ?
			IWL_MAX_SCAN_SIZE : sizeof(struct iwl_cmd);

	phys_addr = pci_map_single(priv->pci_dev, out_cmd,
				   len, PCI_DMA_BIDIRECTIONAL);
	pci_unmap_addr_set(&out_cmd->meta, mapping, phys_addr);
	pci_unmap_len_set(&out_cmd->meta, len, len);
	phys_addr += offsetof(struct iwl_cmd, hdr);

	iwl_hw_txq_attach_buf_to_tfd(priv, tfd, phys_addr, fix_size);

#ifdef CONFIG_IWLWIFI_DEBUG
	switch (out_cmd->hdr.cmd) {
	case REPLY_TX_LINK_QUALITY_CMD:
	case SENSITIVITY_CMD:
		IWL_DEBUG_HC_DUMP("Sending command %s (#%x), seq: 0x%04X, "
				"%d bytes at %d[%d]:%d\n",
				get_cmd_string(out_cmd->hdr.cmd),
				out_cmd->hdr.cmd,
				le16_to_cpu(out_cmd->hdr.sequence), fix_size,
				q->write_ptr, idx, IWL_CMD_QUEUE_NUM);
				break;
	default:
		IWL_DEBUG_HC("Sending command %s (#%x), seq: 0x%04X, "
				"%d bytes at %d[%d]:%d\n",
				get_cmd_string(out_cmd->hdr.cmd),
				out_cmd->hdr.cmd,
				le16_to_cpu(out_cmd->hdr.sequence), fix_size,
				q->write_ptr, idx, IWL_CMD_QUEUE_NUM);
	}
#endif
	txq->need_update = 1;

	/* Set up entry in queue's byte count circular buffer */
	priv->cfg->ops->lib->txq_update_byte_cnt_tbl(priv, txq, 0);

	/* Increment and update queue's write index */
	q->write_ptr = iwl_queue_inc_wrap(q->write_ptr, q->n_bd);
	ret = iwl_txq_update_write_ptr(priv, txq);

	spin_unlock_irqrestore(&priv->hcmd_lock, flags);
	return ret ? ret : idx;
}

int iwl_tx_queue_reclaim(struct iwl_priv *priv, int txq_id, int index)
{
	struct iwl_tx_queue *txq = &priv->txq[txq_id];
	struct iwl_queue *q = &txq->q;
	struct iwl_tx_info *tx_info;
	int nfreed = 0;

	if ((index >= q->n_bd) || (iwl_queue_used(q, index) == 0)) {
		IWL_ERROR("Read index for DMA queue txq id (%d), index %d, "
			  "is out of range [0-%d] %d %d.\n", txq_id,
			  index, q->n_bd, q->write_ptr, q->read_ptr);
		return 0;
	}

	for (index = iwl_queue_inc_wrap(index, q->n_bd);
	     q->read_ptr != index;
	     q->read_ptr = iwl_queue_inc_wrap(q->read_ptr, q->n_bd)) {

		tx_info = &txq->txb[txq->q.read_ptr];
		ieee80211_tx_status_irqsafe(priv->hw, tx_info->skb[0]);
		tx_info->skb[0] = NULL;

		if (priv->cfg->ops->lib->txq_inval_byte_cnt_tbl)
			priv->cfg->ops->lib->txq_inval_byte_cnt_tbl(priv, txq);

		iwl_hw_txq_free_tfd(priv, txq);
		nfreed++;
	}
	return nfreed;
}
EXPORT_SYMBOL(iwl_tx_queue_reclaim);


static void iwl_hcmd_queue_reclaim(struct iwl_priv *priv, int txq_id,
				   int idx, int cmd_idx)
{
	struct iwl_tx_queue *txq = &priv->txq[txq_id];
	struct iwl_queue *q = &txq->q;
	int nfreed = 0;

	if ((idx >= q->n_bd) || (iwl_queue_used(q, idx) == 0)) {
		IWL_ERROR("Read index for DMA queue txq id (%d), index %d, "
			  "is out of range [0-%d] %d %d.\n", txq_id,
			  idx, q->n_bd, q->write_ptr, q->read_ptr);
		return;
	}

	pci_unmap_single(priv->pci_dev,
		pci_unmap_addr(&txq->cmd[cmd_idx]->meta, mapping),
		pci_unmap_len(&txq->cmd[cmd_idx]->meta, len),
		PCI_DMA_BIDIRECTIONAL);

	for (idx = iwl_queue_inc_wrap(idx, q->n_bd); q->read_ptr != idx;
	     q->read_ptr = iwl_queue_inc_wrap(q->read_ptr, q->n_bd)) {

		if (nfreed++ > 0) {
			IWL_ERROR("HCMD skipped: index (%d) %d %d\n", idx,
					q->write_ptr, q->read_ptr);
			queue_work(priv->workqueue, &priv->restart);
		}

	}
}

void iwl_tx_cmd_complete(struct iwl_priv *priv, struct iwl_rx_mem_buffer *rxb)
{
	struct iwl_rx_packet *pkt = (struct iwl_rx_packet *)rxb->skb->data;
	u16 sequence = le16_to_cpu(pkt->hdr.sequence);
	int txq_id = SEQ_TO_QUEUE(sequence);
	int index = SEQ_TO_INDEX(sequence);
	int cmd_index;
	bool huge = !!(pkt->hdr.sequence & SEQ_HUGE_FRAME);
	struct iwl_cmd *cmd;

	/* If a Tx command is being handled and it isn't in the actual
	 * command queue then there a command routing bug has been introduced
	 * in the queue management code. */
	if (WARN(txq_id != IWL_CMD_QUEUE_NUM,
		 "wrong command queue %d, sequence 0x%X readp=%d writep=%d\n",
		  txq_id, sequence,
		  priv->txq[IWL_CMD_QUEUE_NUM].q.read_ptr,
		  priv->txq[IWL_CMD_QUEUE_NUM].q.write_ptr)) {
		iwl_print_hex_dump(priv, IWL_DL_INFO , rxb, 32);
		return;
	}

	cmd_index = get_cmd_index(&priv->txq[IWL_CMD_QUEUE_NUM].q, index, huge);
	cmd = priv->txq[IWL_CMD_QUEUE_NUM].cmd[cmd_index];

	/* Input error checking is done when commands are added to queue. */
	if (cmd->meta.flags & CMD_WANT_SKB) {
		cmd->meta.source->u.skb = rxb->skb;
		rxb->skb = NULL;
	} else if (cmd->meta.u.callback &&
		   !cmd->meta.u.callback(priv, cmd, rxb->skb))
		rxb->skb = NULL;

	iwl_hcmd_queue_reclaim(priv, txq_id, index, cmd_index);

	if (!(cmd->meta.flags & CMD_ASYNC)) {
		clear_bit(STATUS_HCMD_ACTIVE, &priv->status);
		wake_up_interruptible(&priv->wait_command_queue);
	}
}
EXPORT_SYMBOL(iwl_tx_cmd_complete);

static int iwl_txq_ctx_activate_free(struct iwl_priv *priv)
{
	int txq_id;

	for (txq_id = 0; txq_id < priv->hw_params.max_txq_num; txq_id++)
		if (!test_and_set_bit(txq_id, &priv->txq_ctx_active_msk))
			return txq_id;
	return -1;
}

int iwl_tx_agg_start(struct iwl_priv *priv, const u8 *ra, u16 tid, u16 *ssn)
{
	int sta_id;
	int tx_fifo;
	int txq_id;
	int ret;
	unsigned long flags;
	struct iwl_tid_data *tid_data;

	if (likely(tid < ARRAY_SIZE(default_tid_to_tx_fifo)))
		tx_fifo = default_tid_to_tx_fifo[tid];
	else
		return -EINVAL;

	IWL_WARNING("%s on ra = %pM tid = %d\n",
			__func__, ra, tid);

	sta_id = iwl_find_station(priv, ra);
	if (sta_id == IWL_INVALID_STATION)
		return -ENXIO;

	if (priv->stations[sta_id].tid[tid].agg.state != IWL_AGG_OFF) {
		IWL_ERROR("Start AGG when state is not IWL_AGG_OFF !\n");
		return -ENXIO;
	}

	txq_id = iwl_txq_ctx_activate_free(priv);
	if (txq_id == -1)
		return -ENXIO;

	spin_lock_irqsave(&priv->sta_lock, flags);
	tid_data = &priv->stations[sta_id].tid[tid];
	*ssn = SEQ_TO_SN(tid_data->seq_number);
	tid_data->agg.txq_id = txq_id;
	spin_unlock_irqrestore(&priv->sta_lock, flags);

	ret = priv->cfg->ops->lib->txq_agg_enable(priv, txq_id, tx_fifo,
						  sta_id, tid, *ssn);
	if (ret)
		return ret;

	if (tid_data->tfds_in_queue == 0) {
		printk(KERN_ERR "HW queue is empty\n");
		tid_data->agg.state = IWL_AGG_ON;
		ieee80211_start_tx_ba_cb_irqsafe(priv->hw, ra, tid);
	} else {
		IWL_DEBUG_HT("HW queue is NOT empty: %d packets in HW queue\n",
			     tid_data->tfds_in_queue);
		tid_data->agg.state = IWL_EMPTYING_HW_QUEUE_ADDBA;
	}
	return ret;
}
EXPORT_SYMBOL(iwl_tx_agg_start);

int iwl_tx_agg_stop(struct iwl_priv *priv , const u8 *ra, u16 tid)
{
	int tx_fifo_id, txq_id, sta_id, ssn = -1;
	struct iwl_tid_data *tid_data;
	int ret, write_ptr, read_ptr;
	unsigned long flags;

	if (!ra) {
		IWL_ERROR("ra = NULL\n");
		return -EINVAL;
	}

	if (likely(tid < ARRAY_SIZE(default_tid_to_tx_fifo)))
		tx_fifo_id = default_tid_to_tx_fifo[tid];
	else
		return -EINVAL;

	sta_id = iwl_find_station(priv, ra);

	if (sta_id == IWL_INVALID_STATION)
		return -ENXIO;

	if (priv->stations[sta_id].tid[tid].agg.state != IWL_AGG_ON)
		IWL_WARNING("Stopping AGG while state not IWL_AGG_ON\n");

	tid_data = &priv->stations[sta_id].tid[tid];
	ssn = (tid_data->seq_number & IEEE80211_SCTL_SEQ) >> 4;
	txq_id = tid_data->agg.txq_id;
	write_ptr = priv->txq[txq_id].q.write_ptr;
	read_ptr = priv->txq[txq_id].q.read_ptr;

	/* The queue is not empty */
	if (write_ptr != read_ptr) {
		IWL_DEBUG_HT("Stopping a non empty AGG HW QUEUE\n");
		priv->stations[sta_id].tid[tid].agg.state =
				IWL_EMPTYING_HW_QUEUE_DELBA;
		return 0;
	}

	IWL_DEBUG_HT("HW queue is empty\n");
	priv->stations[sta_id].tid[tid].agg.state = IWL_AGG_OFF;

	spin_lock_irqsave(&priv->lock, flags);
	ret = priv->cfg->ops->lib->txq_agg_disable(priv, txq_id, ssn,
						   tx_fifo_id);
	spin_unlock_irqrestore(&priv->lock, flags);

	if (ret)
		return ret;

	ieee80211_stop_tx_ba_cb_irqsafe(priv->hw, ra, tid);

	return 0;
}
EXPORT_SYMBOL(iwl_tx_agg_stop);

int iwl_txq_check_empty(struct iwl_priv *priv, int sta_id, u8 tid, int txq_id)
{
	struct iwl_queue *q = &priv->txq[txq_id].q;
	u8 *addr = priv->stations[sta_id].sta.sta.addr;
	struct iwl_tid_data *tid_data = &priv->stations[sta_id].tid[tid];

	switch (priv->stations[sta_id].tid[tid].agg.state) {
	case IWL_EMPTYING_HW_QUEUE_DELBA:
		/* We are reclaiming the last packet of the */
		/* aggregated HW queue */
		if ((txq_id  == tid_data->agg.txq_id) &&
		    (q->read_ptr == q->write_ptr)) {
			u16 ssn = SEQ_TO_SN(tid_data->seq_number);
			int tx_fifo = default_tid_to_tx_fifo[tid];
			IWL_DEBUG_HT("HW queue empty: continue DELBA flow\n");
			priv->cfg->ops->lib->txq_agg_disable(priv, txq_id,
							     ssn, tx_fifo);
			tid_data->agg.state = IWL_AGG_OFF;
			ieee80211_stop_tx_ba_cb_irqsafe(priv->hw, addr, tid);
		}
		break;
	case IWL_EMPTYING_HW_QUEUE_ADDBA:
		/* We are reclaiming the last packet of the queue */
		if (tid_data->tfds_in_queue == 0) {
			IWL_DEBUG_HT("HW queue empty: continue ADDBA flow\n");
			tid_data->agg.state = IWL_AGG_ON;
			ieee80211_start_tx_ba_cb_irqsafe(priv->hw, addr, tid);
		}
		break;
	}
	return 0;
}
EXPORT_SYMBOL(iwl_txq_check_empty);

static int iwl_tx_status_reply_compressed_ba(struct iwl_priv *priv,
				 struct iwl_ht_agg *agg,
				 struct iwl_compressed_ba_resp *ba_resp)

{
	int i, sh, ack;
	u16 seq_ctl = le16_to_cpu(ba_resp->seq_ctl);
	u16 scd_flow = le16_to_cpu(ba_resp->scd_flow);
	u64 bitmap;
	int successes = 0;
	struct ieee80211_tx_info *info;

	if (unlikely(!agg->wait_for_ba))  {
		IWL_ERROR("Received BA when not expected\n");
		return -EINVAL;
	}

	/* Mark that the expected block-ack response arrived */
	agg->wait_for_ba = 0;
	IWL_DEBUG_TX_REPLY("BA %d %d\n", agg->start_idx, ba_resp->seq_ctl);

	/* Calculate shift to align block-ack bits with our Tx window bits */
	sh = agg->start_idx - SEQ_TO_INDEX(seq_ctl >> 4);
	if (sh < 0) /* tbw something is wrong with indices */
		sh += 0x100;

	/* don't use 64-bit values for now */
	bitmap = le64_to_cpu(ba_resp->bitmap) >> sh;

	if (agg->frame_count > (64 - sh)) {
		IWL_DEBUG_TX_REPLY("more frames than bitmap size");
		return -1;
	}

	/* check for success or failure according to the
	 * transmitted bitmap and block-ack bitmap */
	bitmap &= agg->bitmap;

	/* For each frame attempted in aggregation,
	 * update driver's record of tx frame's status. */
	for (i = 0; i < agg->frame_count ; i++) {
		ack = bitmap & (1ULL << i);
		successes += !!ack;
		IWL_DEBUG_TX_REPLY("%s ON i=%d idx=%d raw=%d\n",
			ack ? "ACK" : "NACK", i, (agg->start_idx + i) & 0xff,
			agg->start_idx + i);
	}

	info = IEEE80211_SKB_CB(priv->txq[scd_flow].txb[agg->start_idx].skb[0]);
	memset(&info->status, 0, sizeof(info->status));
	info->flags = IEEE80211_TX_STAT_ACK;
	info->flags |= IEEE80211_TX_STAT_AMPDU;
	info->status.ampdu_ack_map = successes;
	info->status.ampdu_ack_len = agg->frame_count;
	iwl_hwrate_to_tx_control(priv, agg->rate_n_flags, info);

	IWL_DEBUG_TX_REPLY("Bitmap %llx\n", (unsigned long long)bitmap);

	return 0;
}

void iwl_rx_reply_compressed_ba(struct iwl_priv *priv,
					   struct iwl_rx_mem_buffer *rxb)
{
	struct iwl_rx_packet *pkt = (struct iwl_rx_packet *)rxb->skb->data;
	struct iwl_compressed_ba_resp *ba_resp = &pkt->u.compressed_ba;
	struct iwl_tx_queue *txq = NULL;
	struct iwl_ht_agg *agg;
	int index;
	int sta_id;
	int tid;

	/* "flow" corresponds to Tx queue */
	u16 scd_flow = le16_to_cpu(ba_resp->scd_flow);

	/* "ssn" is start of block-ack Tx window, corresponds to index
	 * (in Tx queue's circular buffer) of first TFD/frame in window */
	u16 ba_resp_scd_ssn = le16_to_cpu(ba_resp->scd_ssn);

	if (scd_flow >= priv->hw_params.max_txq_num) {
		IWL_ERROR("BUG_ON scd_flow is bigger than number of queues\n");
		return;
	}

	txq = &priv->txq[scd_flow];
	sta_id = ba_resp->sta_id;
	tid = ba_resp->tid;
	agg = &priv->stations[sta_id].tid[tid].agg;

	/* Find index just before block-ack window */
	index = iwl_queue_dec_wrap(ba_resp_scd_ssn & 0xff, txq->q.n_bd);

	/* TODO: Need to get this copy more safely - now good for debug */

	IWL_DEBUG_TX_REPLY("REPLY_COMPRESSED_BA [%d] Received from %pM, "
			   "sta_id = %d\n",
			   agg->wait_for_ba,
			   (u8 *) &ba_resp->sta_addr_lo32,
			   ba_resp->sta_id);
	IWL_DEBUG_TX_REPLY("TID = %d, SeqCtl = %d, bitmap = 0x%llx, scd_flow = "
			   "%d, scd_ssn = %d\n",
			   ba_resp->tid,
			   ba_resp->seq_ctl,
			   (unsigned long long)le64_to_cpu(ba_resp->bitmap),
			   ba_resp->scd_flow,
			   ba_resp->scd_ssn);
	IWL_DEBUG_TX_REPLY("DAT start_idx = %d, bitmap = 0x%llx \n",
			   agg->start_idx,
			   (unsigned long long)agg->bitmap);

	/* Update driver's record of ACK vs. not for each frame in window */
	iwl_tx_status_reply_compressed_ba(priv, agg, ba_resp);

	/* Release all TFDs before the SSN, i.e. all TFDs in front of
	 * block-ack window (we assume that they've been successfully
	 * transmitted ... if not, it's too late anyway). */
	if (txq->q.read_ptr != (ba_resp_scd_ssn & 0xff)) {
		/* calculate mac80211 ampdu sw queue to wake */
		int freed = iwl_tx_queue_reclaim(priv, scd_flow, index);
		priv->stations[sta_id].tid[tid].tfds_in_queue -= freed;

		if ((iwl_queue_space(&txq->q) > txq->q.low_mark) &&
		    priv->mac80211_registered &&
		    (agg->state != IWL_EMPTYING_HW_QUEUE_DELBA))
			ieee80211_wake_queue(priv->hw, txq->swq_id);

		iwl_txq_check_empty(priv, sta_id, tid, scd_flow);
	}
}
EXPORT_SYMBOL(iwl_rx_reply_compressed_ba);

#ifdef CONFIG_IWLWIFI_DEBUG
#define TX_STATUS_ENTRY(x) case TX_STATUS_FAIL_ ## x: return #x

const char *iwl_get_tx_fail_reason(u32 status)
{
	switch (status & TX_STATUS_MSK) {
	case TX_STATUS_SUCCESS:
		return "SUCCESS";
		TX_STATUS_ENTRY(SHORT_LIMIT);
		TX_STATUS_ENTRY(LONG_LIMIT);
		TX_STATUS_ENTRY(FIFO_UNDERRUN);
		TX_STATUS_ENTRY(MGMNT_ABORT);
		TX_STATUS_ENTRY(NEXT_FRAG);
		TX_STATUS_ENTRY(LIFE_EXPIRE);
		TX_STATUS_ENTRY(DEST_PS);
		TX_STATUS_ENTRY(ABORTED);
		TX_STATUS_ENTRY(BT_RETRY);
		TX_STATUS_ENTRY(STA_INVALID);
		TX_STATUS_ENTRY(FRAG_DROPPED);
		TX_STATUS_ENTRY(TID_DISABLE);
		TX_STATUS_ENTRY(FRAME_FLUSHED);
		TX_STATUS_ENTRY(INSUFFICIENT_CF_POLL);
		TX_STATUS_ENTRY(TX_LOCKED);
		TX_STATUS_ENTRY(NO_BEACON_ON_RADAR);
	}

	return "UNKNOWN";
}
EXPORT_SYMBOL(iwl_get_tx_fail_reason);
#endif /* CONFIG_IWLWIFI_DEBUG */
