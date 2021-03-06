

#include <linux/module.h>
#include <linux/kernel.h>

#include <linux/init.h>
#include <linux/sched.h>
#include <linux/types.h>
#include <linux/fcntl.h>
#include <linux/interrupt.h>
#include <linux/ptrace.h>
#include <linux/poll.h>

#include <linux/slab.h>
#include <linux/tty.h>
#include <linux/errno.h>
#include <linux/string.h>
#include <linux/signal.h>
#include <linux/ioctl.h>
#include <linux/skbuff.h>

#include <net/bluetooth/bluetooth.h>
#include <net/bluetooth/hci_core.h>

#include "hci_uart.h"

/* HCILL commands */
#define HCILL_GO_TO_SLEEP_IND	0x30
#define HCILL_GO_TO_SLEEP_ACK	0x31
#define HCILL_WAKE_UP_IND	0x32
#define HCILL_WAKE_UP_ACK	0x33

/* HCILL receiver States */
#define HCILL_W4_PACKET_TYPE	0
#define HCILL_W4_EVENT_HDR	1
#define HCILL_W4_ACL_HDR	2
#define HCILL_W4_SCO_HDR	3
#define HCILL_W4_DATA		4

/* HCILL states */
enum hcill_states_e {
	HCILL_ASLEEP,
	HCILL_ASLEEP_TO_AWAKE,
	HCILL_AWAKE,
	HCILL_AWAKE_TO_ASLEEP
};

struct hcill_cmd {
	u8 cmd;
} __attribute__((packed));

struct ll_struct {
	unsigned long rx_state;
	unsigned long rx_count;
	struct sk_buff *rx_skb;
	struct sk_buff_head txq;
	spinlock_t hcill_lock;		/* HCILL state lock	*/
	unsigned long hcill_state;	/* HCILL power state	*/
	struct sk_buff_head tx_wait_q;	/* HCILL wait queue	*/
};

static int send_hcill_cmd(u8 cmd, struct hci_uart *hu)
{
	int err = 0;
	struct sk_buff *skb = NULL;
	struct ll_struct *ll = hu->priv;
	struct hcill_cmd *hcill_packet;

	BT_DBG("hu %p cmd 0x%x", hu, cmd);

	/* allocate packet */
	skb = bt_skb_alloc(1, GFP_ATOMIC);
	if (!skb) {
		BT_ERR("cannot allocate memory for HCILL packet");
		err = -ENOMEM;
		goto out;
	}

	/* prepare packet */
	hcill_packet = (struct hcill_cmd *) skb_put(skb, 1);
	hcill_packet->cmd = cmd;
	skb->dev = (void *) hu->hdev;

	/* send packet */
	skb_queue_tail(&ll->txq, skb);
out:
	return err;
}

/* Initialize protocol */
static int ll_open(struct hci_uart *hu)
{
	struct ll_struct *ll;

	BT_DBG("hu %p", hu);

	ll = kzalloc(sizeof(*ll), GFP_ATOMIC);
	if (!ll)
		return -ENOMEM;

	skb_queue_head_init(&ll->txq);
	skb_queue_head_init(&ll->tx_wait_q);
	spin_lock_init(&ll->hcill_lock);

	ll->hcill_state = HCILL_AWAKE;

	hu->priv = ll;

	return 0;
}

/* Flush protocol data */
static int ll_flush(struct hci_uart *hu)
{
	struct ll_struct *ll = hu->priv;

	BT_DBG("hu %p", hu);

	skb_queue_purge(&ll->tx_wait_q);
	skb_queue_purge(&ll->txq);

	return 0;
}

/* Close protocol */
static int ll_close(struct hci_uart *hu)
{
	struct ll_struct *ll = hu->priv;

	BT_DBG("hu %p", hu);

	skb_queue_purge(&ll->tx_wait_q);
	skb_queue_purge(&ll->txq);

	kfree_skb(ll->rx_skb);

	hu->priv = NULL;

	kfree(ll);

	return 0;
}

static void __ll_do_awake(struct ll_struct *ll)
{
	struct sk_buff *skb = NULL;

	while ((skb = skb_dequeue(&ll->tx_wait_q)))
		skb_queue_tail(&ll->txq, skb);

	ll->hcill_state = HCILL_AWAKE;
}

static void ll_device_want_to_wakeup(struct hci_uart *hu)
{
	unsigned long flags;
	struct ll_struct *ll = hu->priv;

	BT_DBG("hu %p", hu);

	/* lock hcill state */
	spin_lock_irqsave(&ll->hcill_lock, flags);

	switch (ll->hcill_state) {
	case HCILL_ASLEEP_TO_AWAKE:
		/*
		 * This state means that both the host and the BRF chip
		 * have simultaneously sent a wake-up-indication packet.
		 * Traditionaly, in this case, receiving a wake-up-indication
		 * was enough and an additional wake-up-ack wasn't needed.
		 * This has changed with the BRF6350, which does require an
		 * explicit wake-up-ack. Other BRF versions, which do not
		 * require an explicit ack here, do accept it, thus it is
		 * perfectly safe to always send one.
		 */
		BT_DBG("dual wake-up-indication");
		/* deliberate fall-through - do not add break */
	case HCILL_ASLEEP:
		/* acknowledge device wake up */
		if (send_hcill_cmd(HCILL_WAKE_UP_ACK, hu) < 0) {
			BT_ERR("cannot acknowledge device wake up");
			goto out;
		}
		break;
	default:
		/* any other state is illegal */
		BT_ERR("received HCILL_WAKE_UP_IND in state %ld", ll->hcill_state);
		break;
	}

	/* send pending packets and change state to HCILL_AWAKE */
	__ll_do_awake(ll);

out:
	spin_unlock_irqrestore(&ll->hcill_lock, flags);

	/* actually send the packets */
	hci_uart_tx_wakeup(hu);
}

static void ll_device_want_to_sleep(struct hci_uart *hu)
{
	unsigned long flags;
	struct ll_struct *ll = hu->priv;

	BT_DBG("hu %p", hu);

	/* lock hcill state */
	spin_lock_irqsave(&ll->hcill_lock, flags);

	/* sanity check */
	if (ll->hcill_state != HCILL_AWAKE)
		BT_ERR("ERR: HCILL_GO_TO_SLEEP_IND in state %ld", ll->hcill_state);

	/* acknowledge device sleep */
	if (send_hcill_cmd(HCILL_GO_TO_SLEEP_ACK, hu) < 0) {
		BT_ERR("cannot acknowledge device sleep");
		goto out;
	}

	/* update state */
	ll->hcill_state = HCILL_ASLEEP;

out:
	spin_unlock_irqrestore(&ll->hcill_lock, flags);

	/* actually send the sleep ack packet */
	hci_uart_tx_wakeup(hu);
}

static void ll_device_woke_up(struct hci_uart *hu)
{
	unsigned long flags;
	struct ll_struct *ll = hu->priv;

	BT_DBG("hu %p", hu);

	/* lock hcill state */
	spin_lock_irqsave(&ll->hcill_lock, flags);

	/* sanity check */
	if (ll->hcill_state != HCILL_ASLEEP_TO_AWAKE)
		BT_ERR("received HCILL_WAKE_UP_ACK in state %ld", ll->hcill_state);

	/* send pending packets and change state to HCILL_AWAKE */
	__ll_do_awake(ll);

	spin_unlock_irqrestore(&ll->hcill_lock, flags);

	/* actually send the packets */
	hci_uart_tx_wakeup(hu);
}

/* Enqueue frame for transmittion (padding, crc, etc) */
/* may be called from two simultaneous tasklets */
static int ll_enqueue(struct hci_uart *hu, struct sk_buff *skb)
{
	unsigned long flags = 0;
	struct ll_struct *ll = hu->priv;

	BT_DBG("hu %p skb %p", hu, skb);

	/* Prepend skb with frame type */
	memcpy(skb_push(skb, 1), &bt_cb(skb)->pkt_type, 1);

	/* lock hcill state */
	spin_lock_irqsave(&ll->hcill_lock, flags);

	/* act according to current state */
	switch (ll->hcill_state) {
	case HCILL_AWAKE:
		BT_DBG("device awake, sending normally");
		skb_queue_tail(&ll->txq, skb);
		break;
	case HCILL_ASLEEP:
		BT_DBG("device asleep, waking up and queueing packet");
		/* save packet for later */
		skb_queue_tail(&ll->tx_wait_q, skb);
		/* awake device */
		if (send_hcill_cmd(HCILL_WAKE_UP_IND, hu) < 0) {
			BT_ERR("cannot wake up device");
			break;
		}
		ll->hcill_state = HCILL_ASLEEP_TO_AWAKE;
		break;
	case HCILL_ASLEEP_TO_AWAKE:
		BT_DBG("device waking up, queueing packet");
		/* transient state; just keep packet for later */
		skb_queue_tail(&ll->tx_wait_q, skb);
		break;
	default:
		BT_ERR("illegal hcill state: %ld (losing packet)", ll->hcill_state);
		kfree_skb(skb);
		break;
	}

	spin_unlock_irqrestore(&ll->hcill_lock, flags);

	return 0;
}

static inline int ll_check_data_len(struct ll_struct *ll, int len)
{
	register int room = skb_tailroom(ll->rx_skb);

	BT_DBG("len %d room %d", len, room);

	if (!len) {
		hci_recv_frame(ll->rx_skb);
	} else if (len > room) {
		BT_ERR("Data length is too large");
		kfree_skb(ll->rx_skb);
	} else {
		ll->rx_state = HCILL_W4_DATA;
		ll->rx_count = len;
		return len;
	}

	ll->rx_state = HCILL_W4_PACKET_TYPE;
	ll->rx_skb   = NULL;
	ll->rx_count = 0;

	return 0;
}

/* Recv data */
static int ll_recv(struct hci_uart *hu, void *data, int count)
{
	struct ll_struct *ll = hu->priv;
	register char *ptr;
	struct hci_event_hdr *eh;
	struct hci_acl_hdr   *ah;
	struct hci_sco_hdr   *sh;
	register int len, type, dlen;

	BT_DBG("hu %p count %d rx_state %ld rx_count %ld", hu, count, ll->rx_state, ll->rx_count);

	ptr = data;
	while (count) {
		if (ll->rx_count) {
			len = min_t(unsigned int, ll->rx_count, count);
			memcpy(skb_put(ll->rx_skb, len), ptr, len);
			ll->rx_count -= len; count -= len; ptr += len;

			if (ll->rx_count)
				continue;

			switch (ll->rx_state) {
			case HCILL_W4_DATA:
				BT_DBG("Complete data");
				hci_recv_frame(ll->rx_skb);

				ll->rx_state = HCILL_W4_PACKET_TYPE;
				ll->rx_skb = NULL;
				continue;

			case HCILL_W4_EVENT_HDR:
				eh = (struct hci_event_hdr *) ll->rx_skb->data;

				BT_DBG("Event header: evt 0x%2.2x plen %d", eh->evt, eh->plen);

				ll_check_data_len(ll, eh->plen);
				continue;

			case HCILL_W4_ACL_HDR:
				ah = (struct hci_acl_hdr *) ll->rx_skb->data;
				dlen = __le16_to_cpu(ah->dlen);

				BT_DBG("ACL header: dlen %d", dlen);

				ll_check_data_len(ll, dlen);
				continue;

			case HCILL_W4_SCO_HDR:
				sh = (struct hci_sco_hdr *) ll->rx_skb->data;

				BT_DBG("SCO header: dlen %d", sh->dlen);

				ll_check_data_len(ll, sh->dlen);
				continue;
			}
		}

		/* HCILL_W4_PACKET_TYPE */
		switch (*ptr) {
		case HCI_EVENT_PKT:
			BT_DBG("Event packet");
			ll->rx_state = HCILL_W4_EVENT_HDR;
			ll->rx_count = HCI_EVENT_HDR_SIZE;
			type = HCI_EVENT_PKT;
			break;

		case HCI_ACLDATA_PKT:
			BT_DBG("ACL packet");
			ll->rx_state = HCILL_W4_ACL_HDR;
			ll->rx_count = HCI_ACL_HDR_SIZE;
			type = HCI_ACLDATA_PKT;
			break;

		case HCI_SCODATA_PKT:
			BT_DBG("SCO packet");
			ll->rx_state = HCILL_W4_SCO_HDR;
			ll->rx_count = HCI_SCO_HDR_SIZE;
			type = HCI_SCODATA_PKT;
			break;

		/* HCILL signals */
		case HCILL_GO_TO_SLEEP_IND:
			BT_DBG("HCILL_GO_TO_SLEEP_IND packet");
			ll_device_want_to_sleep(hu);
			ptr++; count--;
			continue;

		case HCILL_GO_TO_SLEEP_ACK:
			/* shouldn't happen */
			BT_ERR("received HCILL_GO_TO_SLEEP_ACK (in state %ld)", ll->hcill_state);
			ptr++; count--;
			continue;

		case HCILL_WAKE_UP_IND:
			BT_DBG("HCILL_WAKE_UP_IND packet");
			ll_device_want_to_wakeup(hu);
			ptr++; count--;
			continue;

		case HCILL_WAKE_UP_ACK:
			BT_DBG("HCILL_WAKE_UP_ACK packet");
			ll_device_woke_up(hu);
			ptr++; count--;
			continue;

		default:
			BT_ERR("Unknown HCI packet type %2.2x", (__u8)*ptr);
			hu->hdev->stat.err_rx++;
			ptr++; count--;
			continue;
		};

		ptr++; count--;

		/* Allocate packet */
		ll->rx_skb = bt_skb_alloc(HCI_MAX_FRAME_SIZE, GFP_ATOMIC);
		if (!ll->rx_skb) {
			BT_ERR("Can't allocate mem for new packet");
			ll->rx_state = HCILL_W4_PACKET_TYPE;
			ll->rx_count = 0;
			return 0;
		}

		ll->rx_skb->dev = (void *) hu->hdev;
		bt_cb(ll->rx_skb)->pkt_type = type;
	}

	return count;
}

static struct sk_buff *ll_dequeue(struct hci_uart *hu)
{
	struct ll_struct *ll = hu->priv;
	return skb_dequeue(&ll->txq);
}

static struct hci_uart_proto llp = {
	.id		= HCI_UART_LL,
	.open		= ll_open,
	.close		= ll_close,
	.recv		= ll_recv,
	.enqueue	= ll_enqueue,
	.dequeue	= ll_dequeue,
	.flush		= ll_flush,
};

int ll_init(void)
{
	int err = hci_uart_register_proto(&llp);

	if (!err)
		BT_INFO("HCILL protocol initialized");
	else
		BT_ERR("HCILL protocol registration failed");

	return err;
}

int ll_deinit(void)
{
	return hci_uart_unregister_proto(&llp);
}
