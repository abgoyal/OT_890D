

#include <linux/module.h>

#include <linux/kernel.h>
#include <linux/init.h>
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

#include <mach/mt_bt.h>

#include "hci_uart.h"

#define VERSION "1.2"

struct h4_struct {
	unsigned long rx_state;
	unsigned long rx_count;
	struct sk_buff *rx_skb;
	struct sk_buff_head txq;

    /* added for mt6611 */
	int			      rxAck;
	struct timer_list rxTime;
    unsigned long     last_jiffies;
    unsigned long     ulMagic;
    spinlock_t         ack_lock;
};

/* H4 receiver States */
#define H4_W4_PACKET_TYPE	0
#define H4_W4_EVENT_HDR		1
#define H4_W4_ACL_HDR		2
#define H4_W4_SCO_HDR		3
#define H4_W4_DATA		4

/* Initialize protocol */
static int h4_open(struct hci_uart *hu)
{
	struct h4_struct *h4;

	BT_DBG("hu %p", hu);

	h4 = kzalloc(sizeof(*h4), GFP_ATOMIC);
	if (!h4)
		return -ENOMEM;

	skb_queue_head_init(&h4->txq);

	hu->priv = h4;

	return 0;
}

/* Flush protocol data */
static int h4_flush(struct hci_uart *hu)
{
	struct h4_struct *h4 = hu->priv;

	BT_DBG("hu %p", hu);

	skb_queue_purge(&h4->txq);

	return 0;
}

/* Close protocol */
static int h4_close(struct hci_uart *hu)
{
	struct h4_struct *h4 = hu->priv;

	hu->priv = NULL;

	BT_DBG("hu %p", hu);

	skb_queue_purge(&h4->txq);

	kfree_skb(h4->rx_skb);

	hu->priv = NULL;
	kfree(h4);

	return 0;
}

/* Enqueue frame for transmittion (padding, crc, etc) */
static int h4_enqueue(struct hci_uart *hu, struct sk_buff *skb)
{
	struct h4_struct *h4 = hu->priv;

#if 1
	unsigned long lCurrentTime = 0;/* in msec */
	struct sk_buff * skbMagic = NULL; /* used to store magic skb */
	unsigned char	ucMagic = 0xFF;
#endif
	BT_DBG("hu %p skb %p", hu, skb);
#if 1/* Hanlde ext. interrupt */
    if(bt_cb(skb)->pkt_type == 1 ){
        unsigned short usOpCode = 0;
        usOpCode = (((unsigned short)(skb->data[1])) << 8) | 
                        ((unsigned short)(skb->data[0]));

        BT_DBG_FUNC(" cmd 0x%04x\r\n ", (int)usOpCode);

        if(usOpCode == 0xfcc1){
            BT_INFO_FUNC("Wake host. ACK cmd\r\n ");

	        /* Prepend skb with frame type */
	        memcpy(skb_push(skb, 1), &bt_cb(skb)->pkt_type, 1);
	        skb_queue_head(&h4->txq, skb);
            
            /* because controller has waken host. Host assume device is ready to process. */
            clear_bit(1, &h4->ulMagic); /* allow TX to run */
            return 0;
        }
    }
#endif    

    /* wake up device */
	lCurrentTime = jiffies_to_msecs(h4->last_jiffies);

	if((jiffies_to_msecs(jiffies) - lCurrentTime) > 4 * 1000){
        BT_INFO_FUNC(" h4_enqueue idle more than 4s 0x%08lx 0x%08lx\n", 
			jiffies, h4->last_jiffies);

		/* Allocate packet */
		skbMagic = bt_skb_alloc(HCI_MAX_FRAME_SIZE, GFP_ATOMIC); /* only 0xFF is required */

		if (!skbMagic) {
            BT_ERR_FUNC("Can't allocate mem for new packet"); /* how to handle? */
			return 0;
		}
		skbMagic->pkt_type = 1;
		skbMagic->dev = (void *) hu->hdev;

		memcpy(skb_put(skbMagic, 1), &ucMagic, 1);
		skb_queue_tail(&h4->txq, skbMagic);
	}
	h4->last_jiffies = jiffies;

	/* Prepend skb with frame type */
	memcpy(skb_push(skb, 1), &bt_cb(skb)->pkt_type, 1);
	skb_queue_tail(&h4->txq, skb);

	return 0;
}

static inline int h4_check_data_len(struct hci_uart *hu, int len)
{
	struct h4_struct *h4 = hu->priv;
	register int room = skb_tailroom(h4->rx_skb);

	BT_DBG("len %d room %d", len, room);

	if (!len) {
		hci_recv_frame(h4->rx_skb);
	} else if (len > room) {
		BT_ERR("Data length is too large");
		kfree_skb(h4->rx_skb);
	} else {
		h4->rx_state = H4_W4_DATA;
		h4->rx_count = len;
		return len;
	}

	h4->rx_state = H4_W4_PACKET_TYPE;
	h4->rx_skb   = NULL;
	h4->rx_count = 0;

	return 0;
}

/* Recv data */
static int h4_recv(struct hci_uart *hu, void *data, int count)
{
	struct h4_struct *h4 = hu->priv;
	register char *ptr;
	struct hci_event_hdr *eh;
	struct hci_acl_hdr   *ah;
	struct hci_sco_hdr   *sh;
	register int len, type, dlen;
	int iDiscard = 0;
	int while_count = 0;
    int i_type = 0;

	BT_DBG("hu %p count %d rx_state %ld rx_count %ld", 
			hu, count, h4->rx_state, h4->rx_count);
    BT_DBG_FUNC("h4_recv count %d state %d\n", count, h4->rx_state);
	ptr = data;
	while (count) {
		while_count ++;

		if(while_count > 5000){
            BT_ERR_FUNC("h4_recv while_count %d\n", while_count);
		}

		if (h4->rx_count) {
			len = min_t(unsigned int, h4->rx_count, count);
			memcpy(skb_put(h4->rx_skb, len), ptr, len);
			h4->rx_count -= len; count -= len; ptr += len;

			if (h4->rx_count)
				continue;

			switch (h4->rx_state) {
			case H4_W4_DATA:
                iDiscard = 0; /* default not to drop packet */
                
                i_type = bt_cb(h4->rx_skb)->pkt_type; /* kernel changes HCI packet type to bt_cb */
	            if(HCI_EVENT_PKT == i_type){
		            unsigned short usOpCode = 0;

                    eh = hci_event_hdr(h4->rx_skb);
                   
                    switch(eh->evt){
                        case HCI_EV_CMD_COMPLETE:
		                usOpCode = (((unsigned short)(h4->rx_skb->data[4])) << 8) | 
                        ((unsigned short)(h4->rx_skb->data[3]));

		                if(usOpCode == 0xFCC0){
			                iDiscard = 1;
						    clear_bit(1, &h4->ulMagic); /* allow TX to run */

                            BT_INFO_FUNC("recv event 0x%04x\n", (int)usOpCode);

                            /* flag is cleared. we may resume TX. or later? */
                            hci_uart_tx_wakeup(hu);

		                }
		                if(usOpCode == 0xFCC1){                        
                            BT_INFO_FUNC("recv host awake command event 0x%04x\n", (int)usOpCode);
                            mt_bt_enable_irq();
                        }
                        break;
                        case HCI_EV_CONN_COMPLETE:
                        /* connection complete */
                        BT_INFO_FUNC("conn complete status %d\n", (int)h4->rx_skb->data[2]);
                        break;
                        case HCI_EV_DISCONN_COMPLETE:
                        /* disconnection complete */
                        BT_INFO_FUNC("discconn complete status %d\n", (int)h4->rx_skb->data[2]);
                        break;
                    }
	            }

                if(!iDiscard){
				    hci_recv_frame(h4->rx_skb);
                }else{
                    BT_INFO_FUNC("free skb\n");    
					kfree_skb(h4->rx_skb);
				}

				h4->rx_state = H4_W4_PACKET_TYPE;
				h4->rx_skb = NULL;
				continue;

			case H4_W4_EVENT_HDR:
				eh = hci_event_hdr(h4->rx_skb);

				BT_DBG("Event header: evt 0x%2.2x plen %d", eh->evt, eh->plen);

				h4_check_data_len(hu, eh->plen);
				continue;

			case H4_W4_ACL_HDR:
				ah = hci_acl_hdr(h4->rx_skb);
				dlen = __le16_to_cpu(ah->dlen);

				BT_DBG("ACL header: dlen %d", dlen);
				h4_check_data_len(hu, dlen);
				continue;

			case H4_W4_SCO_HDR:
				sh = hci_sco_hdr(h4->rx_skb);

				BT_DBG("SCO header: dlen %d", sh->dlen);

				h4_check_data_len(hu, sh->dlen);
				continue;
			}
		}

		/* H4_W4_PACKET_TYPE */
		switch (*ptr) {
		case HCI_EVENT_PKT:
			BT_DBG("Event packet");
			h4->rx_state = H4_W4_EVENT_HDR;
			h4->rx_count = HCI_EVENT_HDR_SIZE;
			type = HCI_EVENT_PKT;
			break;

		case HCI_ACLDATA_PKT:
			BT_DBG("ACL packet");
			h4->rx_state = H4_W4_ACL_HDR;
			h4->rx_count = HCI_ACL_HDR_SIZE;
			type = HCI_ACLDATA_PKT;
			break;

		case HCI_SCODATA_PKT:
			BT_DBG("SCO packet");
			h4->rx_state = H4_W4_SCO_HDR;
			h4->rx_count = HCI_SCO_HDR_SIZE;
			type = HCI_SCODATA_PKT;
			break;

		default:
			BT_ERR("Unknown HCI packet type %2.2x", (__u8)*ptr);
			hu->hdev->stat.err_rx++;
			ptr++; count--;
			continue;
		};

		ptr++; count--;
		/* Allocate packet */
		h4->rx_skb = bt_skb_alloc(HCI_MAX_FRAME_SIZE, GFP_ATOMIC);
		if (!h4->rx_skb) {
			BT_ERR("Can't allocate mem for new packet");
			h4->rx_state = H4_W4_PACKET_TYPE;
			h4->rx_count = 0;
			return 0;
		}

		h4->rx_skb->dev = (void *) hu->hdev;
		bt_cb(h4->rx_skb)->pkt_type = type;
	}

	return count;
}

static struct sk_buff *h4_dequeue(struct hci_uart *hu)
{
	struct h4_struct *h4 = hu->priv;
	struct sk_buff *	skb	= NULL;

	if(test_bit(1, &h4->ulMagic)){
        BT_INFO_FUNC(" h4_dequeue magic under going\n");
		return NULL;
	}

	skb = skb_dequeue(&h4->txq);

	if(skb){
		if((skb->pkt_type == 1) && (skb->len == 1) &&  
			(skb->data[0] == 0xFF)){
                BT_INFO_FUNC("h4_dequeue magic number\n");
				set_bit(1, &h4->ulMagic);
		}
	}
	return skb;
}

static struct hci_uart_proto h4p = {
	.id		= HCI_UART_H4,
	.open		= h4_open,
	.close		= h4_close,
	.recv		= h4_recv,
	.enqueue	= h4_enqueue,
	.dequeue	= h4_dequeue,
	.flush		= h4_flush,
};

int h4_init(void)
{
	int err = hci_uart_register_proto(&h4p);

	if (!err)
		BT_INFO("HCI H4 protocol initialized");
	else
		BT_ERR("HCI H4 protocol registration failed");

	return err;
}

int h4_deinit(void)
{
	return hci_uart_unregister_proto(&h4p);
}
