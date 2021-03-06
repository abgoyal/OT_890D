
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/io.h>
#include <linux/errno.h>
#include <linux/stringify.h>
#include <asm/clock.h>
#include <asm/freq.h>

#define N  (-1)
#define NM (-2)
#define ROUND_NEAREST 0
#define ROUND_DOWN -1
#define ROUND_UP   +1

static int adjust_algos[][3] = {
	{},	/* NO_CHANGE */
	{ NM, N, 1 },   /* N:1, N:1 */
	{ 3, 2, 2 },	/* 3:2:2 */
	{ 5, 2, 2 },    /* 5:2:2 */
	{ N, 1, 1 },	/* N:1:1 */

	{ N, 1 },	/* N:1 */

	{ N, 1 },	/* N:1 */
	{ 3, 2 },
	{ 4, 3 },
	{ 5, 4 },

	{ N, 1 }
};

static unsigned long adjust_pair_of_clocks(unsigned long r1, unsigned long r2,
			int m1, int m2, int round_flag)
{
	unsigned long rem, div;
	int the_one = 0;

	pr_debug( "Actual values: r1 = %ld\n", r1);
	pr_debug( "...............r2 = %ld\n", r2);

	if (m1 == m2) {
		r2 = r1;
		pr_debug( "setting equal rates: r2 now %ld\n", r2);
	} else if ((m2 == N  && m1 == 1) ||
		   (m2 == NM && m1 == N)) { /* N:1 or NM:N */
		pr_debug( "Setting rates as 1:N (N:N*M)\n");
		rem = r2 % r1;
		pr_debug( "...remainder = %ld\n", rem);
		if (rem) {
			div = r2 / r1;
			pr_debug( "...div = %ld\n", div);
			switch (round_flag) {
			case ROUND_NEAREST:
				the_one = rem >= r1/2 ? 1 : 0; break;
			case ROUND_UP:
				the_one = 1; break;
			case ROUND_DOWN:
				the_one = 0; break;
			}

			r2 = r1 * (div + the_one);
			pr_debug( "...setting r2 to %ld\n", r2);
		}
	} else if ((m2 == 1  && m1 == N) ||
		   (m2 == N && m1 == NM)) { /* 1:N or N:NM */
		pr_debug( "Setting rates as N:1 (N*M:N)\n");
		rem = r1 % r2;
		pr_debug( "...remainder = %ld\n", rem);
		if (rem) {
			div = r1 / r2;
			pr_debug( "...div = %ld\n", div);
			switch (round_flag) {
			case ROUND_NEAREST:
				the_one = rem > r2/2 ? 1 : 0; break;
			case ROUND_UP:
				the_one = 0; break;
			case ROUND_DOWN:
				the_one = 1; break;
			}

			r2 = r1 / (div + the_one);
			pr_debug( "...setting r2 to %ld\n", r2);
		}
	} else { /* value:value */
		pr_debug( "Setting rates as %d:%d\n", m1, m2);
		div = r1 / m1;
		r2 = div * m2;
		pr_debug( "...div = %ld\n", div);
		pr_debug( "...setting r2 to %ld\n", r2);
	}

	return r2;
}

static void adjust_clocks(int originate, int *l, unsigned long v[],
			  int n_in_line)
{
	int x;

	pr_debug( "Go down from %d...\n", originate);
	/* go up recalculation clocks */
	for (x = originate; x>0; x -- )
		v[x-1] = adjust_pair_of_clocks(v[x], v[x-1],
					l[x], l[x-1],
					ROUND_UP);

	pr_debug( "Go up from %d...\n", originate);
	/* go down recalculation clocks */
	for (x = originate; x<n_in_line - 1; x ++ )
		v[x+1] = adjust_pair_of_clocks(v[x], v[x+1],
					l[x], l[x+1],
					ROUND_UP);
}



static int divisors2[] = { 2, 3, 4, 5, 6, 8, 10, 12, 16, 20, 24, 32, 40 };

static void master_clk_recalc(struct clk *clk)
{
	unsigned frqcr = ctrl_inl(FRQCR);

	clk->rate = CONFIG_SH_PCLK_FREQ * (((frqcr >> 24) & 0x1f) + 1);
}

static void master_clk_init(struct clk *clk)
{
	clk->parent = NULL;
	clk->flags |= CLK_RATE_PROPAGATES;
	clk->rate = CONFIG_SH_PCLK_FREQ;
	master_clk_recalc(clk);
}


static void module_clk_recalc(struct clk *clk)
{
	unsigned long frqcr = ctrl_inl(FRQCR);

	clk->rate = clk->parent->rate / (((frqcr >> 24) & 0x1f) + 1);
}

static int master_clk_setrate(struct clk *clk, unsigned long rate, int id)
{
	int div = rate / clk->rate;
	int master_divs[] = { 2, 3, 4, 6, 8, 16 };
	int index;
	unsigned long frqcr;

	for (index = 1; index < ARRAY_SIZE(master_divs); index++)
		if (div >= master_divs[index - 1] && div < master_divs[index])
			break;

	if (index >= ARRAY_SIZE(master_divs))
		index = ARRAY_SIZE(master_divs);
	div = master_divs[index - 1];

	frqcr = ctrl_inl(FRQCR);
	frqcr &= ~(0xF << 24);
	frqcr |= ( (div-1) << 24);
	ctrl_outl(frqcr, FRQCR);

	return 0;
}

static struct clk_ops sh7722_master_clk_ops = {
	.init = master_clk_init,
	.recalc = master_clk_recalc,
	.set_rate = master_clk_setrate,
};

static struct clk_ops sh7722_module_clk_ops = {
       .recalc = module_clk_recalc,
};

struct frqcr_context {
	unsigned mask;
	unsigned shift;
};

struct frqcr_context sh7722_get_clk_context(const char *name)
{
	struct frqcr_context ctx = { 0, };

	if (!strcmp(name, "peripheral_clk")) {
		ctx.shift = 0;
		ctx.mask = 0xF;
	} else if (!strcmp(name, "sdram_clk")) {
		ctx.shift = 4;
		ctx.mask = 0xF;
	} else if (!strcmp(name, "bus_clk")) {
		ctx.shift = 8;
		ctx.mask = 0xF;
	} else if (!strcmp(name, "sh_clk")) {
		ctx.shift = 12;
		ctx.mask = 0xF;
	} else if (!strcmp(name, "umem_clk")) {
		ctx.shift = 16;
		ctx.mask = 0xF;
	} else if (!strcmp(name, "cpu_clk")) {
		ctx.shift = 20;
		ctx.mask = 7;
	}
	return ctx;
}

static int sh7722_find_div_index(unsigned long parent_rate, unsigned rate)
{
	unsigned div2 = parent_rate * 2 / rate;
	int index;

	if (rate > parent_rate)
		return -EINVAL;

	for (index = 1; index < ARRAY_SIZE(divisors2); index++) {
		if (div2 > divisors2[index - 1] && div2 <= divisors2[index])
			break;
	}
	if (index >= ARRAY_SIZE(divisors2))
		index = ARRAY_SIZE(divisors2) - 1;
	return index;
}

static void sh7722_frqcr_recalc(struct clk *clk)
{
	struct frqcr_context ctx = sh7722_get_clk_context(clk->name);
	unsigned long frqcr = ctrl_inl(FRQCR);
	int index;

	index = (frqcr >> ctx.shift) & ctx.mask;
	clk->rate = clk->parent->rate * 2 / divisors2[index];
}

static int sh7722_frqcr_set_rate(struct clk *clk, unsigned long rate,
				 int algo_id)
{
	struct frqcr_context ctx = sh7722_get_clk_context(clk->name);
	unsigned long parent_rate = clk->parent->rate;
	int div;
	unsigned long frqcr;
	int err = 0;

	/* pretty invalid */
	if (parent_rate < rate)
		return -EINVAL;

	/* look for multiplier/divisor pair */
	div = sh7722_find_div_index(parent_rate, rate);
	if (div<0)
		return div;

	/* calculate new value of clock rate */
	clk->rate = parent_rate * 2 / divisors2[div];
	frqcr = ctrl_inl(FRQCR);

	/* FIXME: adjust as algo_id specifies */
	if (algo_id != NO_CHANGE) {
		int originator;
		char *algo_group_1[] = { "cpu_clk", "umem_clk", "sh_clk" };
		char *algo_group_2[] = { "sh_clk", "bus_clk" };
		char *algo_group_3[] = { "sh_clk", "sdram_clk" };
		char *algo_group_4[] = { "bus_clk", "peripheral_clk" };
		char *algo_group_5[] = { "cpu_clk", "peripheral_clk" };
		char **algo_current = NULL;
		/* 3 is the maximum number of clocks in relation */
		struct clk *ck[3];
		unsigned long values[3]; /* the same comment as above */
		int part_length = -1;
		int i;

		/*
		 * all the steps below only required if adjustion was
		 * requested
		 */
		if (algo_id == IUS_N1_N1 ||
		    algo_id == IUS_322 ||
		    algo_id == IUS_522 ||
		    algo_id == IUS_N11) {
			algo_current = algo_group_1;
			part_length = 3;
		}
		if (algo_id == SB_N1) {
			algo_current = algo_group_2;
			part_length = 2;
		}
		if (algo_id == SB3_N1 ||
		    algo_id == SB3_32 ||
		    algo_id == SB3_43 ||
		    algo_id == SB3_54) {
			algo_current = algo_group_3;
			part_length = 2;
		}
		if (algo_id == BP_N1) {
			algo_current = algo_group_4;
			part_length = 2;
		}
		if (algo_id == IP_N1) {
			algo_current = algo_group_5;
			part_length = 2;
		}
		if (!algo_current)
			goto incorrect_algo_id;

		originator = -1;
		for (i = 0; i < part_length; i ++ ) {
			if (originator >= 0 && !strcmp(clk->name,
						       algo_current[i]))
				originator = i;
			ck[i] = clk_get(NULL, algo_current[i]);
			values[i] = clk_get_rate(ck[i]);
		}

		if (originator >= 0)
			adjust_clocks(originator, adjust_algos[algo_id],
				      values, part_length);

		for (i = 0; i < part_length; i ++ ) {
			struct frqcr_context part_ctx;
			int part_div;

			if (likely(!err)) {
				part_div = sh7722_find_div_index(parent_rate,
								rate);
				if (part_div > 0) {
					part_ctx = sh7722_get_clk_context(
								ck[i]->name);
					frqcr &= ~(part_ctx.mask <<
						   part_ctx.shift);
					frqcr |= part_div << part_ctx.shift;
				} else
					err = part_div;
			}

			ck[i]->ops->recalc(ck[i]);
			clk_put(ck[i]);
		}
	}

	/* was there any error during recalculation ? If so, bail out.. */
	if (unlikely(err!=0))
		goto out_err;

	/* clear FRQCR bits */
	frqcr &= ~(ctx.mask << ctx.shift);
	frqcr |= div << ctx.shift;

	/* ...and perform actual change */
	ctrl_outl(frqcr, FRQCR);
	return 0;

incorrect_algo_id:
	return -EINVAL;
out_err:
	return err;
}

static long sh7722_frqcr_round_rate(struct clk *clk, unsigned long rate)
{
	unsigned long parent_rate = clk->parent->rate;
	int div;

	/* look for multiplier/divisor pair */
	div = sh7722_find_div_index(parent_rate, rate);
	if (div < 0)
		return clk->rate;

	/* calculate new value of clock rate */
	return parent_rate * 2 / divisors2[div];
}

static struct clk_ops sh7722_frqcr_clk_ops = {
	.recalc = sh7722_frqcr_recalc,
	.set_rate = sh7722_frqcr_set_rate,
	.round_rate = sh7722_frqcr_round_rate,
};


#ifndef CONFIG_CPU_SUBTYPE_SH7343

static int sh7722_siu_set_rate(struct clk *clk, unsigned long rate, int algo_id)
{
	unsigned long r;
	int div;

	r = ctrl_inl(clk->arch_flags);
	div = sh7722_find_div_index(clk->parent->rate, rate);
	if (div < 0)
		return div;
	r = (r & ~0xF) | div;
	ctrl_outl(r, clk->arch_flags);
	return 0;
}

static void sh7722_siu_recalc(struct clk *clk)
{
	unsigned long r;

	r = ctrl_inl(clk->arch_flags);
	clk->rate = clk->parent->rate * 2 / divisors2[r & 0xF];
}

static int sh7722_siu_start_stop(struct clk *clk, int enable)
{
	unsigned long r;

	r = ctrl_inl(clk->arch_flags);
	if (enable)
		ctrl_outl(r & ~(1 << 8), clk->arch_flags);
	else
		ctrl_outl(r | (1 << 8), clk->arch_flags);
	return 0;
}

static void sh7722_siu_enable(struct clk *clk)
{
	sh7722_siu_start_stop(clk, 1);
}

static void sh7722_siu_disable(struct clk *clk)
{
	sh7722_siu_start_stop(clk, 0);
}

static struct clk_ops sh7722_siu_clk_ops = {
	.recalc = sh7722_siu_recalc,
	.set_rate = sh7722_siu_set_rate,
	.enable = sh7722_siu_enable,
	.disable = sh7722_siu_disable,
};

#endif /* CONFIG_CPU_SUBTYPE_SH7343 */

static void sh7722_video_enable(struct clk *clk)
{
	unsigned long r;

	r = ctrl_inl(VCLKCR);
	ctrl_outl( r & ~(1<<8), VCLKCR);
}

static void sh7722_video_disable(struct clk *clk)
{
	unsigned long r;

	r = ctrl_inl(VCLKCR);
	ctrl_outl( r | (1<<8), VCLKCR);
}

static int sh7722_video_set_rate(struct clk *clk, unsigned long rate,
				 int algo_id)
{
	unsigned long r;

	r = ctrl_inl(VCLKCR);
	r &= ~0x3F;
	r |= ((clk->parent->rate / rate - 1) & 0x3F);
	ctrl_outl(r, VCLKCR);
	return 0;
}

static void sh7722_video_recalc(struct clk *clk)
{
	unsigned long r;

	r = ctrl_inl(VCLKCR);
	clk->rate = clk->parent->rate / ((r & 0x3F) + 1);
}

static struct clk_ops sh7722_video_clk_ops = {
	.recalc = sh7722_video_recalc,
	.set_rate = sh7722_video_set_rate,
	.enable = sh7722_video_enable,
	.disable = sh7722_video_disable,
};
static struct clk sh7722_umem_clock = {
	.name = "umem_clk",
	.ops = &sh7722_frqcr_clk_ops,
	.flags = CLK_RATE_PROPAGATES,
};

static struct clk sh7722_sh_clock = {
	.name = "sh_clk",
	.ops = &sh7722_frqcr_clk_ops,
	.flags = CLK_RATE_PROPAGATES,
};

static struct clk sh7722_peripheral_clock = {
	.name = "peripheral_clk",
	.ops = &sh7722_frqcr_clk_ops,
	.flags = CLK_RATE_PROPAGATES,
};

static struct clk sh7722_sdram_clock = {
	.name = "sdram_clk",
	.ops = &sh7722_frqcr_clk_ops,
};

static struct clk sh7722_r_clock = {
	.name = "r_clk",
	.rate = 32768,
	.flags = CLK_RATE_PROPAGATES,
};

#ifndef CONFIG_CPU_SUBTYPE_SH7343

static struct clk sh7722_siu_a_clock = {
	.name = "siu_a_clk",
	.arch_flags = SCLKACR,
	.ops = &sh7722_siu_clk_ops,
};

static struct clk sh7722_siu_b_clock = {
	.name = "siu_b_clk",
	.arch_flags = SCLKBCR,
	.ops = &sh7722_siu_clk_ops,
};

#if defined(CONFIG_CPU_SUBTYPE_SH7722)
static struct clk sh7722_irda_clock = {
	.name = "irda_clk",
	.arch_flags = IrDACLKCR,
	.ops = &sh7722_siu_clk_ops,
};
#endif
#endif /* CONFIG_CPU_SUBTYPE_SH7343 */

static struct clk sh7722_video_clock = {
	.name = "video_clk",
	.ops = &sh7722_video_clk_ops,
};

#define MSTPCR_ARCH_FLAGS(reg, bit) (((reg) << 8) | (bit))
#define MSTPCR_ARCH_FLAGS_REG(value) ((value) >> 8)
#define MSTPCR_ARCH_FLAGS_BIT(value) ((value) & 0xff)

static int sh7722_mstpcr_start_stop(struct clk *clk, int enable)
{
	unsigned long bit = MSTPCR_ARCH_FLAGS_BIT(clk->arch_flags);
	unsigned long reg;
	unsigned long r;

	switch(MSTPCR_ARCH_FLAGS_REG(clk->arch_flags)) {
	case 0:
		reg = MSTPCR0;
		break;
	case 1:
		reg = MSTPCR1;
		break;
	case 2:
		reg = MSTPCR2;
		break;
	default:
		return -EINVAL;
	}  

	r = ctrl_inl(reg);

	if (enable)
		r &= ~(1 << bit);
	else
		r |= (1 << bit);

	ctrl_outl(r, reg);
	return 0;
}

static void sh7722_mstpcr_enable(struct clk *clk)
{
	sh7722_mstpcr_start_stop(clk, 1);
}

static void sh7722_mstpcr_disable(struct clk *clk)
{
	sh7722_mstpcr_start_stop(clk, 0);
}

static void sh7722_mstpcr_recalc(struct clk *clk)
{
	if (clk->parent)
		clk->rate = clk->parent->rate;
}

static struct clk_ops sh7722_mstpcr_clk_ops = {
	.enable = sh7722_mstpcr_enable,
	.disable = sh7722_mstpcr_disable,
	.recalc = sh7722_mstpcr_recalc,
};

#define MSTPCR(_name, _parent, regnr, bitnr) \
{						\
	.name = _name,				\
	.arch_flags = MSTPCR_ARCH_FLAGS(regnr, bitnr),	\
	.ops = (void *)_parent,		\
}

static struct clk sh7722_mstpcr_clocks[] = {
#if defined(CONFIG_CPU_SUBTYPE_SH7722)
	MSTPCR("uram0", "umem_clk", 0, 28),
	MSTPCR("xymem0", "bus_clk", 0, 26),
	MSTPCR("tmu0", "peripheral_clk", 0, 15),
	MSTPCR("cmt0", "r_clk", 0, 14),
	MSTPCR("rwdt0", "r_clk", 0, 13),
	MSTPCR("flctl0", "peripheral_clk", 0, 10),
	MSTPCR("scif0", "peripheral_clk", 0, 7),
	MSTPCR("scif1", "peripheral_clk", 0, 6),
	MSTPCR("scif2", "peripheral_clk", 0, 5),
	MSTPCR("i2c0", "peripheral_clk", 1, 9),
	MSTPCR("rtc0", "r_clk", 1, 8),
	MSTPCR("sdhi0", "peripheral_clk", 2, 18),
	MSTPCR("keysc0", "r_clk", 2, 14),
	MSTPCR("usbf0", "peripheral_clk", 2, 11),
	MSTPCR("2dg0", "bus_clk", 2, 9),
	MSTPCR("siu0", "bus_clk", 2, 8),
	MSTPCR("vou0", "bus_clk", 2, 5),
	MSTPCR("jpu0", "bus_clk", 2, 6),
	MSTPCR("beu0", "bus_clk", 2, 4),
	MSTPCR("ceu0", "bus_clk", 2, 3),
	MSTPCR("veu0", "bus_clk", 2, 2),
	MSTPCR("vpu0", "bus_clk", 2, 1),
	MSTPCR("lcdc0", "bus_clk", 2, 0),
#endif
#if defined(CONFIG_CPU_SUBTYPE_SH7723)
	/* See page 60 of Datasheet V1.0: Overview -> Block Diagram */
	MSTPCR("tlb0", "cpu_clk", 0, 31),
	MSTPCR("ic0", "cpu_clk", 0, 30),
	MSTPCR("oc0", "cpu_clk", 0, 29),
	MSTPCR("l2c0", "sh_clk", 0, 28),
	MSTPCR("ilmem0", "cpu_clk", 0, 27),
	MSTPCR("fpu0", "cpu_clk", 0, 24),
	MSTPCR("intc0", "cpu_clk", 0, 22),
	MSTPCR("dmac0", "bus_clk", 0, 21),
	MSTPCR("sh0", "sh_clk", 0, 20),
	MSTPCR("hudi0", "peripheral_clk", 0, 19),
	MSTPCR("ubc0", "cpu_clk", 0, 17),
	MSTPCR("tmu0", "peripheral_clk", 0, 15),
	MSTPCR("cmt0", "r_clk", 0, 14),
	MSTPCR("rwdt0", "r_clk", 0, 13),
	MSTPCR("dmac1", "bus_clk", 0, 12),
	MSTPCR("tmu1", "peripheral_clk", 0, 11),
	MSTPCR("flctl0", "peripheral_clk", 0, 10),
	MSTPCR("scif0", "peripheral_clk", 0, 9),
	MSTPCR("scif1", "peripheral_clk", 0, 8),
	MSTPCR("scif2", "peripheral_clk", 0, 7),
	MSTPCR("scif3", "bus_clk", 0, 6),
	MSTPCR("scif4", "bus_clk", 0, 5),
	MSTPCR("scif5", "bus_clk", 0, 4),
	MSTPCR("msiof0", "bus_clk", 0, 2),
	MSTPCR("msiof1", "bus_clk", 0, 1),
	MSTPCR("meram0", "sh_clk", 0, 0),
	MSTPCR("i2c0", "peripheral_clk", 1, 9),
	MSTPCR("rtc0", "r_clk", 1, 8),
	MSTPCR("atapi0", "sh_clk", 2, 28),
	MSTPCR("adc0", "peripheral_clk", 2, 28),
	MSTPCR("tpu0", "bus_clk", 2, 25),
	MSTPCR("irda0", "peripheral_clk", 2, 24),
	MSTPCR("tsif0", "bus_clk", 2, 22),
	MSTPCR("icb0", "bus_clk", 2, 21),
	MSTPCR("sdhi0", "bus_clk", 2, 18),
	MSTPCR("sdhi1", "bus_clk", 2, 17),
	MSTPCR("keysc0", "r_clk", 2, 14),
	MSTPCR("usb0", "bus_clk", 2, 11),
	MSTPCR("2dg0", "bus_clk", 2, 10),
	MSTPCR("siu0", "bus_clk", 2, 8),
	MSTPCR("veu1", "bus_clk", 2, 6),
	MSTPCR("vou0", "bus_clk", 2, 5),
	MSTPCR("beu0", "bus_clk", 2, 4),
	MSTPCR("ceu0", "bus_clk", 2, 3),
	MSTPCR("veu0", "bus_clk", 2, 2),
	MSTPCR("vpu0", "bus_clk", 2, 1),
	MSTPCR("lcdc0", "bus_clk", 2, 0),
#endif
#if defined(CONFIG_CPU_SUBTYPE_SH7343)
	MSTPCR("uram0", "umem_clk", 0, 28),
	MSTPCR("xymem0", "bus_clk", 0, 26),
	MSTPCR("tmu0", "peripheral_clk", 0, 15),
	MSTPCR("cmt0", "r_clk", 0, 14),
	MSTPCR("rwdt0", "r_clk", 0, 13),
	MSTPCR("scif0", "peripheral_clk", 0, 7),
	MSTPCR("scif1", "peripheral_clk", 0, 6),
	MSTPCR("scif2", "peripheral_clk", 0, 5),
	MSTPCR("scif3", "peripheral_clk", 0, 4),
	MSTPCR("i2c0", "peripheral_clk", 1, 9),
	MSTPCR("i2c1", "peripheral_clk", 1, 8),
	MSTPCR("sdhi0", "peripheral_clk", 2, 18),
	MSTPCR("keysc0", "r_clk", 2, 14),
	MSTPCR("usbf0", "peripheral_clk", 2, 11),
	MSTPCR("siu0", "bus_clk", 2, 8),
	MSTPCR("jpu0", "bus_clk", 2, 6),
	MSTPCR("vou0", "bus_clk", 2, 5),
	MSTPCR("beu0", "bus_clk", 2, 4),
	MSTPCR("ceu0", "bus_clk", 2, 3),
	MSTPCR("veu0", "bus_clk", 2, 2),
	MSTPCR("vpu0", "bus_clk", 2, 1),
	MSTPCR("lcdc0", "bus_clk", 2, 0),
#endif
#if defined(CONFIG_CPU_SUBTYPE_SH7366)
	/* See page 52 of Datasheet V0.40: Overview -> Block Diagram */
	MSTPCR("tlb0", "cpu_clk", 0, 31),
	MSTPCR("ic0", "cpu_clk", 0, 30),
	MSTPCR("oc0", "cpu_clk", 0, 29),
	MSTPCR("rsmem0", "sh_clk", 0, 28),
	MSTPCR("xymem0", "cpu_clk", 0, 26),
	MSTPCR("intc30", "peripheral_clk", 0, 23),
	MSTPCR("intc0", "peripheral_clk", 0, 22),
	MSTPCR("dmac0", "bus_clk", 0, 21),
	MSTPCR("sh0", "sh_clk", 0, 20),
	MSTPCR("hudi0", "peripheral_clk", 0, 19),
	MSTPCR("ubc0", "cpu_clk", 0, 17),
	MSTPCR("tmu0", "peripheral_clk", 0, 15),
	MSTPCR("cmt0", "r_clk", 0, 14),
	MSTPCR("rwdt0", "r_clk", 0, 13),
	MSTPCR("flctl0", "peripheral_clk", 0, 10),
	MSTPCR("scif0", "peripheral_clk", 0, 7),
	MSTPCR("scif1", "bus_clk", 0, 6),
	MSTPCR("scif2", "bus_clk", 0, 5),
	MSTPCR("msiof0", "peripheral_clk", 0, 2),
	MSTPCR("sbr0", "peripheral_clk", 0, 1),
	MSTPCR("i2c0", "peripheral_clk", 1, 9),
	MSTPCR("icb0", "bus_clk", 2, 27),
	MSTPCR("meram0", "sh_clk", 2, 26),
	MSTPCR("dacc0", "peripheral_clk", 2, 24),
	MSTPCR("dacy0", "peripheral_clk", 2, 23),
	MSTPCR("tsif0", "bus_clk", 2, 22),
	MSTPCR("sdhi0", "bus_clk", 2, 18),
	MSTPCR("mmcif0", "bus_clk", 2, 17),
	MSTPCR("usb0", "bus_clk", 2, 11),
	MSTPCR("siu0", "bus_clk", 2, 8),
	MSTPCR("veu1", "bus_clk", 2, 7),
	MSTPCR("vou0", "bus_clk", 2, 5),
	MSTPCR("beu0", "bus_clk", 2, 4),
	MSTPCR("ceu0", "bus_clk", 2, 3),
	MSTPCR("veu0", "bus_clk", 2, 2),
	MSTPCR("vpu0", "bus_clk", 2, 1),
	MSTPCR("lcdc0", "bus_clk", 2, 0),
#endif
};

static struct clk *sh7722_clocks[] = {
	&sh7722_umem_clock,
	&sh7722_sh_clock,
	&sh7722_peripheral_clock,
	&sh7722_sdram_clock,
#ifndef CONFIG_CPU_SUBTYPE_SH7343
	&sh7722_siu_a_clock,
	&sh7722_siu_b_clock,
#if defined(CONFIG_CPU_SUBTYPE_SH7722)
	&sh7722_irda_clock,
#endif
#endif
	&sh7722_video_clock,
};

struct clk_ops *onchip_ops[] = {
	&sh7722_master_clk_ops,
	&sh7722_module_clk_ops,
	&sh7722_frqcr_clk_ops,
	&sh7722_frqcr_clk_ops,
};

void __init
arch_init_clk_ops(struct clk_ops **ops, int type)
{
	BUG_ON(type < 0 || type > ARRAY_SIZE(onchip_ops));
	*ops = onchip_ops[type];
}

int __init arch_clk_init(void)
{
	struct clk *clk;
	int i;

	clk = clk_get(NULL, "master_clk");
	for (i = 0; i < ARRAY_SIZE(sh7722_clocks); i++) {
		pr_debug( "Registering clock '%s'\n", sh7722_clocks[i]->name);
		sh7722_clocks[i]->parent = clk;
		clk_register(sh7722_clocks[i]);
	}
	clk_put(clk);

	clk_register(&sh7722_r_clock);

	for (i = 0; i < ARRAY_SIZE(sh7722_mstpcr_clocks); i++) {
		pr_debug( "Registering mstpcr clock '%s'\n",
			  sh7722_mstpcr_clocks[i].name);
		clk = clk_get(NULL, (void *) sh7722_mstpcr_clocks[i].ops);
		sh7722_mstpcr_clocks[i].parent = clk;
		sh7722_mstpcr_clocks[i].ops = &sh7722_mstpcr_clk_ops;
		clk_register(&sh7722_mstpcr_clocks[i]);
		clk_put(clk);
	}

	clk_recalc_rate(&sh7722_r_clock); /* make sure rate gets propagated */

	return 0;
}
