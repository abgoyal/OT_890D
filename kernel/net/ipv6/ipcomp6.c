
#include <linux/module.h>
#include <net/ip.h>
#include <net/xfrm.h>
#include <net/ipcomp.h>
#include <linux/crypto.h>
#include <linux/err.h>
#include <linux/pfkeyv2.h>
#include <linux/random.h>
#include <linux/percpu.h>
#include <linux/smp.h>
#include <linux/list.h>
#include <linux/vmalloc.h>
#include <linux/rtnetlink.h>
#include <net/icmp.h>
#include <net/ipv6.h>
#include <net/protocol.h>
#include <linux/ipv6.h>
#include <linux/icmpv6.h>
#include <linux/mutex.h>

static void ipcomp6_err(struct sk_buff *skb, struct inet6_skb_parm *opt,
				int type, int code, int offset, __be32 info)
{
	__be32 spi;
	struct ipv6hdr *iph = (struct ipv6hdr*)skb->data;
	struct ip_comp_hdr *ipcomph =
		(struct ip_comp_hdr *)(skb->data + offset);
	struct xfrm_state *x;

	if (type != ICMPV6_DEST_UNREACH && type != ICMPV6_PKT_TOOBIG)
		return;

	spi = htonl(ntohs(ipcomph->cpi));
	x = xfrm_state_lookup(&init_net, (xfrm_address_t *)&iph->daddr, spi, IPPROTO_COMP, AF_INET6);
	if (!x)
		return;

	printk(KERN_DEBUG "pmtu discovery on SA IPCOMP/%08x/%pI6\n",
			spi, &iph->daddr);
	xfrm_state_put(x);
}

static struct xfrm_state *ipcomp6_tunnel_create(struct xfrm_state *x)
{
	struct xfrm_state *t = NULL;

	t = xfrm_state_alloc(&init_net);
	if (!t)
		goto out;

	t->id.proto = IPPROTO_IPV6;
	t->id.spi = xfrm6_tunnel_alloc_spi((xfrm_address_t *)&x->props.saddr);
	if (!t->id.spi)
		goto error;

	memcpy(t->id.daddr.a6, x->id.daddr.a6, sizeof(struct in6_addr));
	memcpy(&t->sel, &x->sel, sizeof(t->sel));
	t->props.family = AF_INET6;
	t->props.mode = x->props.mode;
	memcpy(t->props.saddr.a6, x->props.saddr.a6, sizeof(struct in6_addr));

	if (xfrm_init_state(t))
		goto error;

	atomic_set(&t->tunnel_users, 1);

out:
	return t;

error:
	t->km.state = XFRM_STATE_DEAD;
	xfrm_state_put(t);
	t = NULL;
	goto out;
}

static int ipcomp6_tunnel_attach(struct xfrm_state *x)
{
	int err = 0;
	struct xfrm_state *t = NULL;
	__be32 spi;

	spi = xfrm6_tunnel_spi_lookup((xfrm_address_t *)&x->props.saddr);
	if (spi)
		t = xfrm_state_lookup(&init_net, (xfrm_address_t *)&x->id.daddr,
					      spi, IPPROTO_IPV6, AF_INET6);
	if (!t) {
		t = ipcomp6_tunnel_create(x);
		if (!t) {
			err = -EINVAL;
			goto out;
		}
		xfrm_state_insert(t);
		xfrm_state_hold(t);
	}
	x->tunnel = t;
	atomic_inc(&t->tunnel_users);

out:
	return err;
}

static int ipcomp6_init_state(struct xfrm_state *x)
{
	int err = -EINVAL;

	x->props.header_len = 0;
	switch (x->props.mode) {
	case XFRM_MODE_TRANSPORT:
		break;
	case XFRM_MODE_TUNNEL:
		x->props.header_len += sizeof(struct ipv6hdr);
		break;
	default:
		goto out;
	}

	err = ipcomp_init_state(x);
	if (err)
		goto out;

	if (x->props.mode == XFRM_MODE_TUNNEL) {
		err = ipcomp6_tunnel_attach(x);
		if (err)
			goto error_tunnel;
	}

	err = 0;
out:
	return err;
error_tunnel:
	ipcomp_destroy(x);

	goto out;
}

static const struct xfrm_type ipcomp6_type =
{
	.description	= "IPCOMP6",
	.owner		= THIS_MODULE,
	.proto		= IPPROTO_COMP,
	.init_state	= ipcomp6_init_state,
	.destructor	= ipcomp_destroy,
	.input		= ipcomp_input,
	.output		= ipcomp_output,
	.hdr_offset	= xfrm6_find_1stfragopt,
};

static struct inet6_protocol ipcomp6_protocol =
{
	.handler	= xfrm6_rcv,
	.err_handler	= ipcomp6_err,
	.flags		= INET6_PROTO_NOPOLICY,
};

static int __init ipcomp6_init(void)
{
	if (xfrm_register_type(&ipcomp6_type, AF_INET6) < 0) {
		printk(KERN_INFO "ipcomp6 init: can't add xfrm type\n");
		return -EAGAIN;
	}
	if (inet6_add_protocol(&ipcomp6_protocol, IPPROTO_COMP) < 0) {
		printk(KERN_INFO "ipcomp6 init: can't add protocol\n");
		xfrm_unregister_type(&ipcomp6_type, AF_INET6);
		return -EAGAIN;
	}
	return 0;
}

static void __exit ipcomp6_fini(void)
{
	if (inet6_del_protocol(&ipcomp6_protocol, IPPROTO_COMP) < 0)
		printk(KERN_INFO "ipv6 ipcomp close: can't remove protocol\n");
	if (xfrm_unregister_type(&ipcomp6_type, AF_INET6) < 0)
		printk(KERN_INFO "ipv6 ipcomp close: can't remove xfrm type\n");
}

module_init(ipcomp6_init);
module_exit(ipcomp6_fini);
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("IP Payload Compression Protocol (IPComp) for IPv6 - RFC3173");
MODULE_AUTHOR("Mitsuru KANDA <mk@linux-ipv6.org>");

MODULE_ALIAS_XFRM_TYPE(AF_INET6, XFRM_PROTO_COMP);
