

#include <linux/module.h>
#include <linux/netdevice.h>
#include <linux/skbuff.h>
#include <net/datalink.h>
#include <net/llc.h>
#include <net/psnap.h>
#include <linux/mm.h>
#include <linux/in.h>
#include <linux/init.h>
#include <linux/rculist.h>

static LIST_HEAD(snap_list);
static DEFINE_SPINLOCK(snap_lock);
static struct llc_sap *snap_sap;

static struct datalink_proto *find_snap_client(unsigned char *desc)
{
	struct datalink_proto *proto = NULL, *p;

	list_for_each_entry_rcu(p, &snap_list, node) {
		if (!memcmp(p->type, desc, 5)) {
			proto = p;
			break;
		}
	}
	return proto;
}

static int snap_rcv(struct sk_buff *skb, struct net_device *dev,
		    struct packet_type *pt, struct net_device *orig_dev)
{
	int rc = 1;
	struct datalink_proto *proto;
	static struct packet_type snap_packet_type = {
		.type = __constant_htons(ETH_P_SNAP),
	};

	if (unlikely(!pskb_may_pull(skb, 5)))
		goto drop;

	rcu_read_lock();
	proto = find_snap_client(skb_transport_header(skb));
	if (proto) {
		/* Pass the frame on. */
		skb->transport_header += 5;
		skb_pull_rcsum(skb, 5);
		rc = proto->rcvfunc(skb, dev, &snap_packet_type, orig_dev);
	}
	rcu_read_unlock();

	if (unlikely(!proto))
		goto drop;

out:
	return rc;

drop:
	kfree_skb(skb);
	goto out;
}

static int snap_request(struct datalink_proto *dl,
			struct sk_buff *skb, u8 *dest)
{
	memcpy(skb_push(skb, 5), dl->type, 5);
	llc_build_and_send_ui_pkt(snap_sap, skb, dest, snap_sap->laddr.lsap);
	return 0;
}

EXPORT_SYMBOL(register_snap_client);
EXPORT_SYMBOL(unregister_snap_client);

static char snap_err_msg[] __initdata =
	KERN_CRIT "SNAP - unable to register with 802.2\n";

static int __init snap_init(void)
{
	snap_sap = llc_sap_open(0xAA, snap_rcv);

	if (!snap_sap)
		printk(snap_err_msg);

	return 0;
}

module_init(snap_init);

static void __exit snap_exit(void)
{
	llc_sap_put(snap_sap);
}

module_exit(snap_exit);


struct datalink_proto *register_snap_client(unsigned char *desc,
					    int (*rcvfunc)(struct sk_buff *,
							   struct net_device *,
							   struct packet_type *,
							   struct net_device *))
{
	struct datalink_proto *proto = NULL;

	spin_lock_bh(&snap_lock);

	if (find_snap_client(desc))
		goto out;

	proto = kmalloc(sizeof(*proto), GFP_ATOMIC);
	if (proto) {
		memcpy(proto->type, desc,5);
		proto->rcvfunc		= rcvfunc;
		proto->header_length	= 5 + 3; /* snap + 802.2 */
		proto->request		= snap_request;
		list_add_rcu(&proto->node, &snap_list);
	}
out:
	spin_unlock_bh(&snap_lock);

	synchronize_net();
	return proto;
}

void unregister_snap_client(struct datalink_proto *proto)
{
	spin_lock_bh(&snap_lock);
	list_del_rcu(&proto->node);
	spin_unlock_bh(&snap_lock);

	synchronize_net();

	kfree(proto);
}

MODULE_LICENSE("GPL");
