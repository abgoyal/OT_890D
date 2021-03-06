


#include <linux/module.h>
#include <linux/netfilter/x_tables.h>
#include <linux/netfilter_bridge/ebtables.h>
#include <linux/netfilter_bridge/ebt_mark_t.h>

static unsigned int
ebt_mark_tg(struct sk_buff *skb, const struct xt_target_param *par)
{
	const struct ebt_mark_t_info *info = par->targinfo;
	int action = info->target & -16;

	if (action == MARK_SET_VALUE)
		skb->mark = info->mark;
	else if (action == MARK_OR_VALUE)
		skb->mark |= info->mark;
	else if (action == MARK_AND_VALUE)
		skb->mark &= info->mark;
	else
		skb->mark ^= info->mark;

	return info->target | ~EBT_VERDICT_BITS;
}

static bool ebt_mark_tg_check(const struct xt_tgchk_param *par)
{
	const struct ebt_mark_t_info *info = par->targinfo;
	int tmp;

	tmp = info->target | ~EBT_VERDICT_BITS;
	if (BASE_CHAIN && tmp == EBT_RETURN)
		return false;
	if (tmp < -NUM_STANDARD_TARGETS || tmp >= 0)
		return false;
	tmp = info->target & ~EBT_VERDICT_BITS;
	if (tmp != MARK_SET_VALUE && tmp != MARK_OR_VALUE &&
	    tmp != MARK_AND_VALUE && tmp != MARK_XOR_VALUE)
		return false;
	return true;
}

static struct xt_target ebt_mark_tg_reg __read_mostly = {
	.name		= "mark",
	.revision	= 0,
	.family		= NFPROTO_BRIDGE,
	.target		= ebt_mark_tg,
	.checkentry	= ebt_mark_tg_check,
	.targetsize	= XT_ALIGN(sizeof(struct ebt_mark_t_info)),
	.me		= THIS_MODULE,
};

static int __init ebt_mark_init(void)
{
	return xt_register_target(&ebt_mark_tg_reg);
}

static void __exit ebt_mark_fini(void)
{
	xt_unregister_target(&ebt_mark_tg_reg);
}

module_init(ebt_mark_init);
module_exit(ebt_mark_fini);
MODULE_DESCRIPTION("Ebtables: Packet mark modification");
MODULE_LICENSE("GPL");
