

#include <linux/module.h>
#include <linux/skbuff.h>
#include <linux/spinlock.h>
#include <linux/interrupt.h>

#include <linux/netfilter/x_tables.h>
#include <linux/netfilter/xt_limit.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Herve Eychenne <rv@wallfire.org>");
MODULE_DESCRIPTION("Xtables: rate-limit match");
MODULE_ALIAS("ipt_limit");
MODULE_ALIAS("ip6t_limit");


static DEFINE_SPINLOCK(limit_lock);

#define MAX_CPJ (0xFFFFFFFF / (HZ*60*60*24))

#define _POW2_BELOW2(x) ((x)|((x)>>1))
#define _POW2_BELOW4(x) (_POW2_BELOW2(x)|_POW2_BELOW2((x)>>2))
#define _POW2_BELOW8(x) (_POW2_BELOW4(x)|_POW2_BELOW4((x)>>4))
#define _POW2_BELOW16(x) (_POW2_BELOW8(x)|_POW2_BELOW8((x)>>8))
#define _POW2_BELOW32(x) (_POW2_BELOW16(x)|_POW2_BELOW16((x)>>16))
#define POW2_BELOW32(x) ((_POW2_BELOW32(x)>>1) + 1)

#define CREDITS_PER_JIFFY POW2_BELOW32(MAX_CPJ)

static bool
limit_mt(const struct sk_buff *skb, const struct xt_match_param *par)
{
	struct xt_rateinfo *r =
		((const struct xt_rateinfo *)par->matchinfo)->master;
	unsigned long now = jiffies;

	spin_lock_bh(&limit_lock);
	r->credit += (now - xchg(&r->prev, now)) * CREDITS_PER_JIFFY;
	if (r->credit > r->credit_cap)
		r->credit = r->credit_cap;

	if (r->credit >= r->cost) {
		/* We're not limited. */
		r->credit -= r->cost;
		spin_unlock_bh(&limit_lock);
		return true;
	}

	spin_unlock_bh(&limit_lock);
	return false;
}

/* Precision saver. */
static u_int32_t
user2credits(u_int32_t user)
{
	/* If multiplying would overflow... */
	if (user > 0xFFFFFFFF / (HZ*CREDITS_PER_JIFFY))
		/* Divide first. */
		return (user / XT_LIMIT_SCALE) * HZ * CREDITS_PER_JIFFY;

	return (user * HZ * CREDITS_PER_JIFFY) / XT_LIMIT_SCALE;
}

static bool limit_mt_check(const struct xt_mtchk_param *par)
{
	struct xt_rateinfo *r = par->matchinfo;

	/* Check for overflow. */
	if (r->burst == 0
	    || user2credits(r->avg * r->burst) < user2credits(r->avg)) {
		printk("Overflow in xt_limit, try lower: %u/%u\n",
		       r->avg, r->burst);
		return false;
	}

	/* For SMP, we only want to use one set of counters. */
	r->master = r;
	if (r->cost == 0) {
		/* User avg in seconds * XT_LIMIT_SCALE: convert to jiffies *
		   128. */
		r->prev = jiffies;
		r->credit = user2credits(r->avg * r->burst);	 /* Credits full. */
		r->credit_cap = user2credits(r->avg * r->burst); /* Credits full. */
		r->cost = user2credits(r->avg);
	}
	return true;
}

#ifdef CONFIG_COMPAT
struct compat_xt_rateinfo {
	u_int32_t avg;
	u_int32_t burst;

	compat_ulong_t prev;
	u_int32_t credit;
	u_int32_t credit_cap, cost;

	u_int32_t master;
};

static void limit_mt_compat_from_user(void *dst, void *src)
{
	const struct compat_xt_rateinfo *cm = src;
	struct xt_rateinfo m = {
		.avg		= cm->avg,
		.burst		= cm->burst,
		.prev		= cm->prev | (unsigned long)cm->master << 32,
		.credit		= cm->credit,
		.credit_cap	= cm->credit_cap,
		.cost		= cm->cost,
	};
	memcpy(dst, &m, sizeof(m));
}

static int limit_mt_compat_to_user(void __user *dst, void *src)
{
	const struct xt_rateinfo *m = src;
	struct compat_xt_rateinfo cm = {
		.avg		= m->avg,
		.burst		= m->burst,
		.prev		= m->prev,
		.credit		= m->credit,
		.credit_cap	= m->credit_cap,
		.cost		= m->cost,
		.master		= m->prev >> 32,
	};
	return copy_to_user(dst, &cm, sizeof(cm)) ? -EFAULT : 0;
}
#endif /* CONFIG_COMPAT */

static struct xt_match limit_mt_reg __read_mostly = {
	.name             = "limit",
	.revision         = 0,
	.family           = NFPROTO_UNSPEC,
	.match            = limit_mt,
	.checkentry       = limit_mt_check,
	.matchsize        = sizeof(struct xt_rateinfo),
#ifdef CONFIG_COMPAT
	.compatsize       = sizeof(struct compat_xt_rateinfo),
	.compat_from_user = limit_mt_compat_from_user,
	.compat_to_user   = limit_mt_compat_to_user,
#endif
	.me               = THIS_MODULE,
};

static int __init limit_mt_init(void)
{
	return xt_register_match(&limit_mt_reg);
}

static void __exit limit_mt_exit(void)
{
	xt_unregister_match(&limit_mt_reg);
}

module_init(limit_mt_init);
module_exit(limit_mt_exit);
