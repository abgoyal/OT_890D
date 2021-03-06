
#include <linux/init.h>
#include <linux/kernel.h>
#include <asm/clock.h>
#include <asm/freq.h>
#include <asm/io.h>

static int md_table[] = { 1, 2, 3, 4, 6, 8, 12 };

static void master_clk_init(struct clk *clk)
{
	clk->rate *= md_table[ctrl_inw(FRQCR) & 0x0007];
}

static struct clk_ops sh7710_master_clk_ops = {
	.init		= master_clk_init,
};

static void module_clk_recalc(struct clk *clk)
{
	int idx = (ctrl_inw(FRQCR) & 0x0007);
	clk->rate = clk->parent->rate / md_table[idx];
}

static struct clk_ops sh7710_module_clk_ops = {
	.recalc		= module_clk_recalc,
};

static void bus_clk_recalc(struct clk *clk)
{
	int idx = (ctrl_inw(FRQCR) & 0x0700) >> 8;
	clk->rate = clk->parent->rate / md_table[idx];
}

static struct clk_ops sh7710_bus_clk_ops = {
	.recalc		= bus_clk_recalc,
};

static void cpu_clk_recalc(struct clk *clk)
{
	int idx = (ctrl_inw(FRQCR) & 0x0070) >> 4;
	clk->rate = clk->parent->rate / md_table[idx];
}

static struct clk_ops sh7710_cpu_clk_ops = {
	.recalc		= cpu_clk_recalc,
};

static struct clk_ops *sh7710_clk_ops[] = {
	&sh7710_master_clk_ops,
	&sh7710_module_clk_ops,
	&sh7710_bus_clk_ops,
	&sh7710_cpu_clk_ops,
};

void __init arch_init_clk_ops(struct clk_ops **ops, int idx)
{
	if (idx < ARRAY_SIZE(sh7710_clk_ops))
		*ops = sh7710_clk_ops[idx];
}

