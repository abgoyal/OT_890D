
#include <linux/module.h>
#include <linux/skbuff.h>
#include <linux/ip.h>
#include <net/checksum.h>

MODULE_AUTHOR("Henrik Nordstrom <hno@marasystems.com>");
MODULE_DESCRIPTION("Xtables: connection mark modification");
MODULE_LICENSE("GPL");
MODULE_ALIAS("ipt_CONNMARK");
MODULE_ALIAS("ip6t_CONNMARK");

#include <linux/netfilter/x_tables.h>
#include <linux/netfilter/xt_CONNMARK.h>
#include <net/netfilter/nf_conntrack_ecache.h>

static unsigned int
connmark_tg_v0(struct sk_buff *skb, const struct xt_target_param *par)
{
	const struct xt_connmark_target_info *markinfo = par->targinfo;
	struct nf_conn *ct;
	enum ip_conntrack_info ctinfo;
	u_int32_t diff;
	u_int32_t mark;
	u_int32_t newmark;

	ct = nf_ct_get(skb, &ctinfo);
	if (ct) {
		switch(markinfo->mode) {
		case XT_CONNMARK_SET:
			newmark = (ct->mark & ~markinfo->mask) | markinfo->mark;
			if (newmark != ct->mark) {
				ct->mark = newmark;
				nf_conntrack_event_cache(IPCT_MARK, ct);
			}
			break;
		case XT_CONNMARK_SAVE:
			newmark = (ct->mark & ~markinfo->mask) |
				  (skb->mark & markinfo->mask);
			if (ct->mark != newmark) {
				ct->mark = newmark;
				nf_conntrack_event_cache(IPCT_MARK, ct);
			}
			break;
		case XT_CONNMARK_RESTORE:
			mark = skb->mark;
			diff = (ct->mark ^ mark) & markinfo->mask;
			skb->mark = mark ^ diff;
			break;
		}
	}

	return XT_CONTINUE;
}

static unsigned int
connmark_tg(struct sk_buff *skb, const struct xt_target_param *par)
{
	const struct xt_connmark_tginfo1 *info = par->targinfo;
	enum ip_conntrack_info ctinfo;
	struct nf_conn *ct;
	u_int32_t newmark;

	ct = nf_ct_get(skb, &ctinfo);
	if (ct == NULL)
		return XT_CONTINUE;

	switch (info->mode) {
	case XT_CONNMARK_SET:
		newmark = (ct->mark & ~info->ctmask) ^ info->ctmark;
		if (ct->mark != newmark) {
			ct->mark = newmark;
			nf_conntrack_event_cache(IPCT_MARK, ct);
		}
		break;
	case XT_CONNMARK_SAVE:
		newmark = (ct->mark & ~info->ctmask) ^
		          (skb->mark & info->nfmask);
		if (ct->mark != newmark) {
			ct->mark = newmark;
			nf_conntrack_event_cache(IPCT_MARK, ct);
		}
		break;
	case XT_CONNMARK_RESTORE:
		newmark = (skb->mark & ~info->nfmask) ^
		          (ct->mark & info->ctmask);
		skb->mark = newmark;
		break;
	}

	return XT_CONTINUE;
}

static bool connmark_tg_check_v0(const struct xt_tgchk_param *par)
{
	const struct xt_connmark_target_info *matchinfo = par->targinfo;

	if (matchinfo->mode == XT_CONNMARK_RESTORE) {
		if (strcmp(par->table, "mangle") != 0) {
			printk(KERN_WARNING "CONNMARK: restore can only be "
			       "called from \"mangle\" table, not \"%s\"\n",
			       par->table);
			return false;
		}
	}
	if (matchinfo->mark > 0xffffffff || matchinfo->mask > 0xffffffff) {
		printk(KERN_WARNING "CONNMARK: Only supports 32bit mark\n");
		return false;
	}
	if (nf_ct_l3proto_try_module_get(par->family) < 0) {
		printk(KERN_WARNING "can't load conntrack support for "
				    "proto=%u\n", par->family);
		return false;
	}
	return true;
}

static bool connmark_tg_check(const struct xt_tgchk_param *par)
{
	if (nf_ct_l3proto_try_module_get(par->family) < 0) {
		printk(KERN_WARNING "cannot load conntrack support for "
		       "proto=%u\n", par->family);
		return false;
	}
	return true;
}

static void connmark_tg_destroy(const struct xt_tgdtor_param *par)
{
	nf_ct_l3proto_module_put(par->family);
}

#ifdef CONFIG_COMPAT
struct compat_xt_connmark_target_info {
	compat_ulong_t	mark, mask;
	u_int8_t	mode;
	u_int8_t	__pad1;
	u_int16_t	__pad2;
};

static void connmark_tg_compat_from_user_v0(void *dst, void *src)
{
	const struct compat_xt_connmark_target_info *cm = src;
	struct xt_connmark_target_info m = {
		.mark	= cm->mark,
		.mask	= cm->mask,
		.mode	= cm->mode,
	};
	memcpy(dst, &m, sizeof(m));
}

static int connmark_tg_compat_to_user_v0(void __user *dst, void *src)
{
	const struct xt_connmark_target_info *m = src;
	struct compat_xt_connmark_target_info cm = {
		.mark	= m->mark,
		.mask	= m->mask,
		.mode	= m->mode,
	};
	return copy_to_user(dst, &cm, sizeof(cm)) ? -EFAULT : 0;
}
#endif /* CONFIG_COMPAT */

static struct xt_target connmark_tg_reg[] __read_mostly = {
	{
		.name		= "CONNMARK",
		.revision	= 0,
		.family		= NFPROTO_UNSPEC,
		.checkentry	= connmark_tg_check_v0,
		.destroy	= connmark_tg_destroy,
		.target		= connmark_tg_v0,
		.targetsize	= sizeof(struct xt_connmark_target_info),
#ifdef CONFIG_COMPAT
		.compatsize	= sizeof(struct compat_xt_connmark_target_info),
		.compat_from_user = connmark_tg_compat_from_user_v0,
		.compat_to_user	= connmark_tg_compat_to_user_v0,
#endif
		.me		= THIS_MODULE
	},
	{
		.name           = "CONNMARK",
		.revision       = 1,
		.family         = NFPROTO_UNSPEC,
		.checkentry     = connmark_tg_check,
		.target         = connmark_tg,
		.targetsize     = sizeof(struct xt_connmark_tginfo1),
		.destroy        = connmark_tg_destroy,
		.me             = THIS_MODULE,
	},
};

static int __init connmark_tg_init(void)
{
	return xt_register_targets(connmark_tg_reg,
	       ARRAY_SIZE(connmark_tg_reg));
}

static void __exit connmark_tg_exit(void)
{
	xt_unregister_targets(connmark_tg_reg, ARRAY_SIZE(connmark_tg_reg));
}

module_init(connmark_tg_init);
module_exit(connmark_tg_exit);
