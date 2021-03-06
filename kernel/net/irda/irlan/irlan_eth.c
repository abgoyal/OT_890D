

#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/inetdevice.h>
#include <linux/if_arp.h>
#include <linux/module.h>
#include <net/arp.h>

#include <net/irda/irda.h>
#include <net/irda/irmod.h>
#include <net/irda/irlan_common.h>
#include <net/irda/irlan_client.h>
#include <net/irda/irlan_event.h>
#include <net/irda/irlan_eth.h>

static int  irlan_eth_open(struct net_device *dev);
static int  irlan_eth_close(struct net_device *dev);
static int  irlan_eth_xmit(struct sk_buff *skb, struct net_device *dev);
static void irlan_eth_set_multicast_list( struct net_device *dev);
static struct net_device_stats *irlan_eth_get_stats(struct net_device *dev);

static void irlan_eth_setup(struct net_device *dev)
{
	dev->open               = irlan_eth_open;
	dev->stop               = irlan_eth_close;
	dev->hard_start_xmit    = irlan_eth_xmit;
	dev->get_stats	        = irlan_eth_get_stats;
	dev->set_multicast_list = irlan_eth_set_multicast_list;
	dev->destructor		= free_netdev;

	ether_setup(dev);

	/*
	 * Lets do all queueing in IrTTP instead of this device driver.
	 * Queueing here as well can introduce some strange latency
	 * problems, which we will avoid by setting the queue size to 0.
	 */
	/*
	 * The bugs in IrTTP and IrLAN that created this latency issue
	 * have now been fixed, and we can propagate flow control properly
	 * to the network layer. However, this requires a minimal queue of
	 * packets for the device.
	 * Without flow control, the Tx Queue is 14 (ttp) + 0 (dev) = 14
	 * With flow control, the Tx Queue is 7 (ttp) + 4 (dev) = 11
	 * See irlan_eth_flow_indication()...
	 * Note : this number was randomly selected and would need to
	 * be adjusted.
	 * Jean II */
	dev->tx_queue_len = 4;
}

struct net_device *alloc_irlandev(const char *name)
{
	return alloc_netdev(sizeof(struct irlan_cb), name,
			    irlan_eth_setup);
}

static int irlan_eth_open(struct net_device *dev)
{
	struct irlan_cb *self = netdev_priv(dev);

	IRDA_DEBUG(2, "%s()\n", __func__ );

	/* Ready to play! */
	netif_stop_queue(dev); /* Wait until data link is ready */

	/* We are now open, so time to do some work */
	self->disconnect_reason = 0;
	irlan_client_wakeup(self, self->saddr, self->daddr);

	/* Make sure we have a hardware address before we return,
	   so DHCP clients gets happy */
	return wait_event_interruptible(self->open_wait,
					!self->tsap_data->connected);
}

static int irlan_eth_close(struct net_device *dev)
{
	struct irlan_cb *self = netdev_priv(dev);

	IRDA_DEBUG(2, "%s()\n", __func__ );

	/* Stop device */
	netif_stop_queue(dev);

	irlan_close_data_channel(self);
	irlan_close_tsaps(self);

	irlan_do_client_event(self, IRLAN_LMP_DISCONNECT, NULL);
	irlan_do_provider_event(self, IRLAN_LMP_DISCONNECT, NULL);

	/* Remove frames queued on the control channel */
	skb_queue_purge(&self->client.txq);

	self->client.tx_busy = 0;

	return 0;
}

static int irlan_eth_xmit(struct sk_buff *skb, struct net_device *dev)
{
	struct irlan_cb *self = netdev_priv(dev);
	int ret;

	/* skb headroom large enough to contain all IrDA-headers? */
	if ((skb_headroom(skb) < self->max_header_size) || (skb_shared(skb))) {
		struct sk_buff *new_skb =
			skb_realloc_headroom(skb, self->max_header_size);

		/*  We have to free the original skb anyway */
		dev_kfree_skb(skb);

		/* Did the realloc succeed? */
		if (new_skb == NULL)
			return 0;

		/* Use the new skb instead */
		skb = new_skb;
	}

	dev->trans_start = jiffies;

	/* Now queue the packet in the transport layer */
	if (self->use_udata)
		ret = irttp_udata_request(self->tsap_data, skb);
	else
		ret = irttp_data_request(self->tsap_data, skb);

	if (ret < 0) {
		/*
		 * IrTTPs tx queue is full, so we just have to
		 * drop the frame! You might think that we should
		 * just return -1 and don't deallocate the frame,
		 * but that is dangerous since it's possible that
		 * we have replaced the original skb with a new
		 * one with larger headroom, and that would really
		 * confuse do_dev_queue_xmit() in dev.c! I have
		 * tried :-) DB
		 */
		/* irttp_data_request already free the packet */
		self->stats.tx_dropped++;
	} else {
		self->stats.tx_packets++;
		self->stats.tx_bytes += skb->len;
	}

	return 0;
}

int irlan_eth_receive(void *instance, void *sap, struct sk_buff *skb)
{
	struct irlan_cb *self = instance;

	if (skb == NULL) {
		++self->stats.rx_dropped;
		return 0;
	}
	if (skb->len < ETH_HLEN) {
		IRDA_DEBUG(0, "%s() : IrLAN frame too short (%d)\n",
			   __func__, skb->len);
		++self->stats.rx_dropped;
		dev_kfree_skb(skb);
		return 0;
	}

	/*
	 * Adopt this frame! Important to set all these fields since they
	 * might have been previously set by the low level IrDA network
	 * device driver
	 */
	skb->protocol = eth_type_trans(skb, self->dev); /* Remove eth header */

	self->stats.rx_packets++;
	self->stats.rx_bytes += skb->len;

	netif_rx(skb);   /* Eat it! */

	return 0;
}

void irlan_eth_flow_indication(void *instance, void *sap, LOCAL_FLOW flow)
{
	struct irlan_cb *self;
	struct net_device *dev;

	self = (struct irlan_cb *) instance;

	IRDA_ASSERT(self != NULL, return;);
	IRDA_ASSERT(self->magic == IRLAN_MAGIC, return;);

	dev = self->dev;

	IRDA_ASSERT(dev != NULL, return;);

	IRDA_DEBUG(0, "%s() : flow %s ; running %d\n", __func__,
		   flow == FLOW_STOP ? "FLOW_STOP" : "FLOW_START",
		   netif_running(dev));

	switch (flow) {
	case FLOW_STOP:
		/* IrTTP is full, stop higher layers */
		netif_stop_queue(dev);
		break;
	case FLOW_START:
	default:
		/* Tell upper layers that its time to transmit frames again */
		/* Schedule network layer */
		netif_wake_queue(dev);
		break;
	}
}

#define HW_MAX_ADDRS 4 /* Must query to get it! */
static void irlan_eth_set_multicast_list(struct net_device *dev)
{
	struct irlan_cb *self = netdev_priv(dev);

	IRDA_DEBUG(2, "%s()\n", __func__ );

	/* Check if data channel has been connected yet */
	if (self->client.state != IRLAN_DATA) {
		IRDA_DEBUG(1, "%s(), delaying!\n", __func__ );
		return;
	}

	if (dev->flags & IFF_PROMISC) {
		/* Enable promiscuous mode */
		IRDA_WARNING("Promiscuous mode not implemented by IrLAN!\n");
	}
	else if ((dev->flags & IFF_ALLMULTI) || dev->mc_count > HW_MAX_ADDRS) {
		/* Disable promiscuous mode, use normal mode. */
		IRDA_DEBUG(4, "%s(), Setting multicast filter\n", __func__ );
		/* hardware_set_filter(NULL); */

		irlan_set_multicast_filter(self, TRUE);
	}
	else if (dev->mc_count) {
		IRDA_DEBUG(4, "%s(), Setting multicast filter\n", __func__ );
		/* Walk the address list, and load the filter */
		/* hardware_set_filter(dev->mc_list); */

		irlan_set_multicast_filter(self, TRUE);
	}
	else {
		IRDA_DEBUG(4, "%s(), Clearing multicast filter\n", __func__ );
		irlan_set_multicast_filter(self, FALSE);
	}

	if (dev->flags & IFF_BROADCAST)
		irlan_set_broadcast_filter(self, TRUE);
	else
		irlan_set_broadcast_filter(self, FALSE);
}

static struct net_device_stats *irlan_eth_get_stats(struct net_device *dev)
{
	struct irlan_cb *self = netdev_priv(dev);

	return &self->stats;
}
