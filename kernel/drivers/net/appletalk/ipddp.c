

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/ip.h>
#include <linux/atalk.h>
#include <linux/if_arp.h>
#include <net/route.h>
#include <asm/uaccess.h>

#include "ipddp.h"		/* Our stuff */

static const char version[] = KERN_INFO "ipddp.c:v0.01 8/28/97 Bradford W. Johnson <johns393@maroon.tc.umn.edu>\n";

static struct ipddp_route *ipddp_route_list;

#ifdef CONFIG_IPDDP_ENCAP
static int ipddp_mode = IPDDP_ENCAP;
#else
static int ipddp_mode = IPDDP_DECAP;
#endif

/* Index to functions, as function prototypes. */
static int ipddp_xmit(struct sk_buff *skb, struct net_device *dev);
static int ipddp_create(struct ipddp_route *new_rt);
static int ipddp_delete(struct ipddp_route *rt);
static struct ipddp_route* ipddp_find_route(struct ipddp_route *rt);
static int ipddp_ioctl(struct net_device *dev, struct ifreq *ifr, int cmd);

static const struct net_device_ops ipddp_netdev_ops = {
	.ndo_start_xmit		= ipddp_xmit,
	.ndo_do_ioctl   	= ipddp_ioctl,
	.ndo_change_mtu		= eth_change_mtu,
	.ndo_set_mac_address 	= eth_mac_addr,
	.ndo_validate_addr	= eth_validate_addr,
};

static struct net_device * __init ipddp_init(void)
{
	static unsigned version_printed;
	struct net_device *dev;
	int err;

	dev = alloc_etherdev(0);
	if (!dev)
		return ERR_PTR(-ENOMEM);

	strcpy(dev->name, "ipddp%d");

	if (version_printed++ == 0)
                printk(version);

	/* Initalize the device structure. */
	dev->netdev_ops = &ipddp_netdev_ops;

        dev->type = ARPHRD_IPDDP;       	/* IP over DDP tunnel */
        dev->mtu = 585;
        dev->flags |= IFF_NOARP;

        /*
         *      The worst case header we will need is currently a
         *      ethernet header (14 bytes) and a ddp header (sizeof ddpehdr+1)
         *      We send over SNAP so that takes another 8 bytes.
         */
        dev->hard_header_len = 14+8+sizeof(struct ddpehdr)+1;

	err = register_netdev(dev);
	if (err) {
		free_netdev(dev);
		return ERR_PTR(err);
	}

	/* Let the user now what mode we are in */
	if(ipddp_mode == IPDDP_ENCAP)
		printk("%s: Appletalk-IP Encap. mode by Bradford W. Johnson <johns393@maroon.tc.umn.edu>\n", 
			dev->name);
	if(ipddp_mode == IPDDP_DECAP)
		printk("%s: Appletalk-IP Decap. mode by Jay Schulist <jschlst@samba.org>\n", 
			dev->name);

        return dev;
}


static int ipddp_xmit(struct sk_buff *skb, struct net_device *dev)
{
	__be32 paddr = ((struct rtable*)skb->dst)->rt_gateway;
        struct ddpehdr *ddp;
        struct ipddp_route *rt;
        struct atalk_addr *our_addr;

	/*
         * Find appropriate route to use, based only on IP number.
         */
        for(rt = ipddp_route_list; rt != NULL; rt = rt->next)
        {
                if(rt->ip == paddr)
                        break;
        }
        if(rt == NULL)
                return 0;

        our_addr = atalk_find_dev_addr(rt->dev);

	if(ipddp_mode == IPDDP_DECAP)
		/* 
		 * Pull off the excess room that should not be there.
		 * This is due to a hard-header problem. This is the
		 * quick fix for now though, till it breaks.
		 */
		skb_pull(skb, 35-(sizeof(struct ddpehdr)+1));

	/* Create the Extended DDP header */
	ddp = (struct ddpehdr *)skb->data;
        ddp->deh_len_hops = htons(skb->len + (1<<10));
        ddp->deh_sum = 0;

	/*
         * For Localtalk we need aarp_send_ddp to strip the
         * long DDP header and place a shot DDP header on it.
         */
        if(rt->dev->type == ARPHRD_LOCALTLK)
        {
                ddp->deh_dnet  = 0;   /* FIXME more hops?? */
                ddp->deh_snet  = 0;
        }
        else
        {
                ddp->deh_dnet  = rt->at.s_net;   /* FIXME more hops?? */
                ddp->deh_snet  = our_addr->s_net;
        }
        ddp->deh_dnode = rt->at.s_node;
        ddp->deh_snode = our_addr->s_node;
        ddp->deh_dport = 72;
        ddp->deh_sport = 72;

        *((__u8 *)(ddp+1)) = 22;        	/* ddp type = IP */

        skb->protocol = htons(ETH_P_ATALK);     /* Protocol has changed */

	dev->stats.tx_packets++;
	dev->stats.tx_bytes += skb->len;

        if(aarp_send_ddp(rt->dev, skb, &rt->at, NULL) < 0)
                dev_kfree_skb(skb);

        return 0;
}

static int ipddp_create(struct ipddp_route *new_rt)
{
        struct ipddp_route *rt = kmalloc(sizeof(*rt), GFP_KERNEL);

        if (rt == NULL)
                return -ENOMEM;

        rt->ip = new_rt->ip;
        rt->at = new_rt->at;
        rt->next = NULL;
        if ((rt->dev = atrtr_get_dev(&rt->at)) == NULL) {
		kfree(rt);
                return -ENETUNREACH;
        }

	if (ipddp_find_route(rt)) {
		kfree(rt);
		return -EEXIST;
	}

        rt->next = ipddp_route_list;
        ipddp_route_list = rt;

        return 0;
}

static int ipddp_delete(struct ipddp_route *rt)
{
        struct ipddp_route **r = &ipddp_route_list;
        struct ipddp_route *tmp;

        while((tmp = *r) != NULL)
        {
                if(tmp->ip == rt->ip
                        && tmp->at.s_net == rt->at.s_net
                        && tmp->at.s_node == rt->at.s_node)
                {
                        *r = tmp->next;
                        kfree(tmp);
                        return 0;
                }
                r = &tmp->next;
        }

        return (-ENOENT);
}

static struct ipddp_route* ipddp_find_route(struct ipddp_route *rt)
{
        struct ipddp_route *f;

        for(f = ipddp_route_list; f != NULL; f = f->next)
        {
                if(f->ip == rt->ip
                        && f->at.s_net == rt->at.s_net
                        && f->at.s_node == rt->at.s_node)
                        return (f);
        }

        return (NULL);
}

static int ipddp_ioctl(struct net_device *dev, struct ifreq *ifr, int cmd)
{
        struct ipddp_route __user *rt = ifr->ifr_data;
        struct ipddp_route rcp;

        if(!capable(CAP_NET_ADMIN))
                return -EPERM;

	if(copy_from_user(&rcp, rt, sizeof(rcp)))
		return -EFAULT;

        switch(cmd)
        {
		case SIOCADDIPDDPRT:
                        return (ipddp_create(&rcp));

                case SIOCFINDIPDDPRT:
                        if(copy_to_user(rt, ipddp_find_route(&rcp), sizeof(struct ipddp_route)))
                                return -EFAULT;
                        return 0;

                case SIOCDELIPDDPRT:
                        return (ipddp_delete(&rcp));

                default:
                        return -EINVAL;
        }
}

static struct net_device *dev_ipddp;

MODULE_LICENSE("GPL");
module_param(ipddp_mode, int, 0);

static int __init ipddp_init_module(void)
{
	dev_ipddp = ipddp_init();
        if (IS_ERR(dev_ipddp))
                return PTR_ERR(dev_ipddp);
	return 0;
}

static void __exit ipddp_cleanup_module(void)
{
        struct ipddp_route *p;

	unregister_netdev(dev_ipddp);
        free_netdev(dev_ipddp);

        while (ipddp_route_list) {
                p = ipddp_route_list->next;
                kfree(ipddp_route_list);
                ipddp_route_list = p;
        }
}

module_init(ipddp_init_module);
module_exit(ipddp_cleanup_module);