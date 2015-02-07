

#include <net/tipc/tipc.h>
#include <net/tipc/tipc_bearer.h>
#include <net/tipc/tipc_msg.h>
#include <linux/netdevice.h>
#include <net/net_namespace.h>

#define MAX_ETH_BEARERS		2
#define ETH_LINK_PRIORITY	TIPC_DEF_LINK_PRI
#define ETH_LINK_TOLERANCE	TIPC_DEF_LINK_TOL
#define ETH_LINK_WINDOW		TIPC_DEF_LINK_WIN


struct eth_bearer {
	struct tipc_bearer *bearer;
	struct net_device *dev;
	struct packet_type tipc_packet_type;
};

static struct eth_bearer eth_bearers[MAX_ETH_BEARERS];
static int eth_started = 0;
static struct notifier_block notifier;


static int send_msg(struct sk_buff *buf, struct tipc_bearer *tb_ptr,
		    struct tipc_media_addr *dest)
{
	struct sk_buff *clone;
	struct net_device *dev;

	clone = skb_clone(buf, GFP_ATOMIC);
	if (clone) {
		skb_reset_network_header(clone);
		dev = ((struct eth_bearer *)(tb_ptr->usr_handle))->dev;
		clone->dev = dev;
		dev_hard_header(clone, dev, ETH_P_TIPC,
				 &dest->dev_addr.eth_addr,
				 dev->dev_addr, clone->len);
		dev_queue_xmit(clone);
	}
	return 0;
}


static int recv_msg(struct sk_buff *buf, struct net_device *dev,
		    struct packet_type *pt, struct net_device *orig_dev)
{
	struct eth_bearer *eb_ptr = (struct eth_bearer *)pt->af_packet_priv;
	u32 size;

	if (!net_eq(dev_net(dev), &init_net)) {
		kfree_skb(buf);
		return 0;
	}

	if (likely(eb_ptr->bearer)) {
		if (likely(buf->pkt_type <= PACKET_BROADCAST)) {
			size = msg_size((struct tipc_msg *)buf->data);
			skb_trim(buf, size);
			if (likely(buf->len == size)) {
				buf->next = NULL;
				tipc_recv_msg(buf, eb_ptr->bearer);
				return 0;
			}
		}
	}
	kfree_skb(buf);
	return 0;
}


static int enable_bearer(struct tipc_bearer *tb_ptr)
{
	struct net_device *dev = NULL;
	struct net_device *pdev = NULL;
	struct eth_bearer *eb_ptr = &eth_bearers[0];
	struct eth_bearer *stop = &eth_bearers[MAX_ETH_BEARERS];
	char *driver_name = strchr((const char *)tb_ptr->name, ':') + 1;

	/* Find device with specified name */

	for_each_netdev(&init_net, pdev){
		if (!strncmp(pdev->name, driver_name, IFNAMSIZ)) {
			dev = pdev;
			break;
		}
	}
	if (!dev)
		return -ENODEV;

	/* Find Ethernet bearer for device (or create one) */

	for (;(eb_ptr != stop) && eb_ptr->dev && (eb_ptr->dev != dev); eb_ptr++);
	if (eb_ptr == stop)
		return -EDQUOT;
	if (!eb_ptr->dev) {
		eb_ptr->dev = dev;
		eb_ptr->tipc_packet_type.type = htons(ETH_P_TIPC);
		eb_ptr->tipc_packet_type.dev = dev;
		eb_ptr->tipc_packet_type.func = recv_msg;
		eb_ptr->tipc_packet_type.af_packet_priv = eb_ptr;
		INIT_LIST_HEAD(&(eb_ptr->tipc_packet_type.list));
		dev_hold(dev);
		dev_add_pack(&eb_ptr->tipc_packet_type);
	}

	/* Associate TIPC bearer with Ethernet bearer */

	eb_ptr->bearer = tb_ptr;
	tb_ptr->usr_handle = (void *)eb_ptr;
	tb_ptr->mtu = dev->mtu;
	tb_ptr->blocked = 0;
	tb_ptr->addr.type = htonl(TIPC_MEDIA_TYPE_ETH);
	memcpy(&tb_ptr->addr.dev_addr, &dev->dev_addr, ETH_ALEN);
	return 0;
}


static void disable_bearer(struct tipc_bearer *tb_ptr)
{
	((struct eth_bearer *)tb_ptr->usr_handle)->bearer = NULL;
}


static int recv_notification(struct notifier_block *nb, unsigned long evt,
			     void *dv)
{
	struct net_device *dev = (struct net_device *)dv;
	struct eth_bearer *eb_ptr = &eth_bearers[0];
	struct eth_bearer *stop = &eth_bearers[MAX_ETH_BEARERS];

	if (!net_eq(dev_net(dev), &init_net))
		return NOTIFY_DONE;

	while ((eb_ptr->dev != dev)) {
		if (++eb_ptr == stop)
			return NOTIFY_DONE;	/* couldn't find device */
	}
	if (!eb_ptr->bearer)
		return NOTIFY_DONE;		/* bearer had been disabled */

	eb_ptr->bearer->mtu = dev->mtu;

	switch (evt) {
	case NETDEV_CHANGE:
		if (netif_carrier_ok(dev))
			tipc_continue(eb_ptr->bearer);
		else
			tipc_block_bearer(eb_ptr->bearer->name);
		break;
	case NETDEV_UP:
		tipc_continue(eb_ptr->bearer);
		break;
	case NETDEV_DOWN:
		tipc_block_bearer(eb_ptr->bearer->name);
		break;
	case NETDEV_CHANGEMTU:
	case NETDEV_CHANGEADDR:
		tipc_block_bearer(eb_ptr->bearer->name);
		tipc_continue(eb_ptr->bearer);
		break;
	case NETDEV_UNREGISTER:
	case NETDEV_CHANGENAME:
		tipc_disable_bearer(eb_ptr->bearer->name);
		break;
	}
	return NOTIFY_OK;
}


static char *eth_addr2str(struct tipc_media_addr *a, char *str_buf, int str_size)
{
	unchar *addr = (unchar *)&a->dev_addr;

	if (str_size < 18)
		*str_buf = '\0';
	else
		sprintf(str_buf, "%pM", addr);
	return str_buf;
}


int tipc_eth_media_start(void)
{
	struct tipc_media_addr bcast_addr;
	int res;

	if (eth_started)
		return -EINVAL;

	bcast_addr.type = htonl(TIPC_MEDIA_TYPE_ETH);
	memset(&bcast_addr.dev_addr, 0xff, ETH_ALEN);

	memset(eth_bearers, 0, sizeof(eth_bearers));

	res = tipc_register_media(TIPC_MEDIA_TYPE_ETH, "eth",
				  enable_bearer, disable_bearer, send_msg,
				  eth_addr2str, &bcast_addr, ETH_LINK_PRIORITY,
				  ETH_LINK_TOLERANCE, ETH_LINK_WINDOW);
	if (res)
		return res;

	notifier.notifier_call = &recv_notification;
	notifier.priority = 0;
	res = register_netdevice_notifier(&notifier);
	if (!res)
		eth_started = 1;
	return res;
}


void tipc_eth_media_stop(void)
{
	int i;

	if (!eth_started)
		return;

	unregister_netdevice_notifier(&notifier);
	for (i = 0; i < MAX_ETH_BEARERS ; i++) {
		if (eth_bearers[i].bearer) {
			eth_bearers[i].bearer->blocked = 1;
			eth_bearers[i].bearer = NULL;
		}
		if (eth_bearers[i].dev) {
			dev_remove_pack(&eth_bearers[i].tipc_packet_type);
			dev_put(eth_bearers[i].dev);
		}
	}
	memset(&eth_bearers, 0, sizeof(eth_bearers));
	eth_started = 0;
}