
#include <linux/init.h>
#include <linux/ioport.h>
#include <linux/delay.h>
#include <linux/param.h>
#include <linux/ptrace.h>
#include <linux/mtd/physmap.h>
#include <asm/reboot.h>
#include <asm/traps.h>
#include <asm/txx9irq.h>
#include <asm/txx9tmr.h>
#include <asm/txx9pio.h>
#include <asm/txx9/generic.h>
#include <asm/txx9/tx4927.h>

static void __init tx4927_wdr_init(void)
{
	/* report watchdog reset status */
	if (____raw_readq(&tx4927_ccfgptr->ccfg) & TX4927_CCFG_WDRST)
		pr_warning("Watchdog reset detected at 0x%lx\n",
			   read_c0_errorepc());
	/* clear WatchDogReset (W1C) */
	tx4927_ccfg_set(TX4927_CCFG_WDRST);
	/* do reset on watchdog */
	tx4927_ccfg_set(TX4927_CCFG_WR);
}

void __init tx4927_wdt_init(void)
{
	txx9_wdt_init(TX4927_TMR_REG(2) & 0xfffffffffULL);
}

static void tx4927_machine_restart(char *command)
{
	local_irq_disable();
	pr_emerg("Rebooting (with %s watchdog reset)...\n",
		 (____raw_readq(&tx4927_ccfgptr->ccfg) & TX4927_CCFG_WDREXEN) ?
		 "external" : "internal");
	/* clear watchdog status */
	tx4927_ccfg_set(TX4927_CCFG_WDRST);	/* W1C */
	txx9_wdt_now(TX4927_TMR_REG(2) & 0xfffffffffULL);
	while (!(____raw_readq(&tx4927_ccfgptr->ccfg) & TX4927_CCFG_WDRST))
		;
	mdelay(10);
	if (____raw_readq(&tx4927_ccfgptr->ccfg) & TX4927_CCFG_WDREXEN) {
		pr_emerg("Rebooting (with internal watchdog reset)...\n");
		/* External WDRST failed.  Do internal watchdog reset */
		tx4927_ccfg_clear(TX4927_CCFG_WDREXEN);
	}
	/* fallback */
	(*_machine_halt)();
}

void show_registers(struct pt_regs *regs);
static int tx4927_be_handler(struct pt_regs *regs, int is_fixup)
{
	int data = regs->cp0_cause & 4;
	console_verbose();
	pr_err("%cBE exception at %#lx\n", data ? 'D' : 'I', regs->cp0_epc);
	pr_err("ccfg:%llx, toea:%llx\n",
	       (unsigned long long)____raw_readq(&tx4927_ccfgptr->ccfg),
	       (unsigned long long)____raw_readq(&tx4927_ccfgptr->toea));
#ifdef CONFIG_PCI
	tx4927_report_pcic_status();
#endif
	show_registers(regs);
	panic("BusError!");
}
static void __init tx4927_be_init(void)
{
	board_be_handler = tx4927_be_handler;
}

static struct resource tx4927_sdram_resource[4];

void __init tx4927_setup(void)
{
	int i;
	__u32 divmode;
	int cpuclk = 0;
	u64 ccfg;

	txx9_reg_res_init(TX4927_REV_PCODE(), TX4927_REG_BASE,
			  TX4927_REG_SIZE);
	set_c0_config(TX49_CONF_CWFON);

	/* SDRAMC,EBUSC are configured by PROM */
	for (i = 0; i < 8; i++) {
		if (!(TX4927_EBUSC_CR(i) & 0x8))
			continue;	/* disabled */
		txx9_ce_res[i].start = (unsigned long)TX4927_EBUSC_BA(i);
		txx9_ce_res[i].end =
			txx9_ce_res[i].start + TX4927_EBUSC_SIZE(i) - 1;
		request_resource(&iomem_resource, &txx9_ce_res[i]);
	}

	/* clocks */
	ccfg = ____raw_readq(&tx4927_ccfgptr->ccfg);
	if (txx9_master_clock) {
		/* calculate gbus_clock and cpu_clock from master_clock */
		divmode = (__u32)ccfg & TX4927_CCFG_DIVMODE_MASK;
		switch (divmode) {
		case TX4927_CCFG_DIVMODE_8:
		case TX4927_CCFG_DIVMODE_10:
		case TX4927_CCFG_DIVMODE_12:
		case TX4927_CCFG_DIVMODE_16:
			txx9_gbus_clock = txx9_master_clock * 4; break;
		default:
			txx9_gbus_clock = txx9_master_clock;
		}
		switch (divmode) {
		case TX4927_CCFG_DIVMODE_2:
		case TX4927_CCFG_DIVMODE_8:
			cpuclk = txx9_gbus_clock * 2; break;
		case TX4927_CCFG_DIVMODE_2_5:
		case TX4927_CCFG_DIVMODE_10:
			cpuclk = txx9_gbus_clock * 5 / 2; break;
		case TX4927_CCFG_DIVMODE_3:
		case TX4927_CCFG_DIVMODE_12:
			cpuclk = txx9_gbus_clock * 3; break;
		case TX4927_CCFG_DIVMODE_4:
		case TX4927_CCFG_DIVMODE_16:
			cpuclk = txx9_gbus_clock * 4; break;
		}
		txx9_cpu_clock = cpuclk;
	} else {
		if (txx9_cpu_clock == 0)
			txx9_cpu_clock = 200000000;	/* 200MHz */
		/* calculate gbus_clock and master_clock from cpu_clock */
		cpuclk = txx9_cpu_clock;
		divmode = (__u32)ccfg & TX4927_CCFG_DIVMODE_MASK;
		switch (divmode) {
		case TX4927_CCFG_DIVMODE_2:
		case TX4927_CCFG_DIVMODE_8:
			txx9_gbus_clock = cpuclk / 2; break;
		case TX4927_CCFG_DIVMODE_2_5:
		case TX4927_CCFG_DIVMODE_10:
			txx9_gbus_clock = cpuclk * 2 / 5; break;
		case TX4927_CCFG_DIVMODE_3:
		case TX4927_CCFG_DIVMODE_12:
			txx9_gbus_clock = cpuclk / 3; break;
		case TX4927_CCFG_DIVMODE_4:
		case TX4927_CCFG_DIVMODE_16:
			txx9_gbus_clock = cpuclk / 4; break;
		}
		switch (divmode) {
		case TX4927_CCFG_DIVMODE_8:
		case TX4927_CCFG_DIVMODE_10:
		case TX4927_CCFG_DIVMODE_12:
		case TX4927_CCFG_DIVMODE_16:
			txx9_master_clock = txx9_gbus_clock / 4; break;
		default:
			txx9_master_clock = txx9_gbus_clock;
		}
	}
	/* change default value to udelay/mdelay take reasonable time */
	loops_per_jiffy = txx9_cpu_clock / HZ / 2;

	/* CCFG */
	tx4927_wdr_init();
	/* clear BusErrorOnWrite flag (W1C) */
	tx4927_ccfg_set(TX4927_CCFG_BEOW);
	/* enable Timeout BusError */
	if (txx9_ccfg_toeon)
		tx4927_ccfg_set(TX4927_CCFG_TOE);

	/* DMA selection */
	txx9_clear64(&tx4927_ccfgptr->pcfg, TX4927_PCFG_DMASEL_ALL);

	/* Use external clock for external arbiter */
	if (!(____raw_readq(&tx4927_ccfgptr->ccfg) & TX4927_CCFG_PCIARB))
		txx9_clear64(&tx4927_ccfgptr->pcfg, TX4927_PCFG_PCICLKEN_ALL);

	printk(KERN_INFO "%s -- %dMHz(M%dMHz) CRIR:%08x CCFG:%llx PCFG:%llx\n",
	       txx9_pcode_str,
	       (cpuclk + 500000) / 1000000,
	       (txx9_master_clock + 500000) / 1000000,
	       (__u32)____raw_readq(&tx4927_ccfgptr->crir),
	       (unsigned long long)____raw_readq(&tx4927_ccfgptr->ccfg),
	       (unsigned long long)____raw_readq(&tx4927_ccfgptr->pcfg));

	printk(KERN_INFO "%s SDRAMC --", txx9_pcode_str);
	for (i = 0; i < 4; i++) {
		__u64 cr = TX4927_SDRAMC_CR(i);
		unsigned long base, size;
		if (!((__u32)cr & 0x00000400))
			continue;	/* disabled */
		base = (unsigned long)(cr >> 49) << 21;
		size = (((unsigned long)(cr >> 33) & 0x7fff) + 1) << 21;
		printk(" CR%d:%016llx", i, (unsigned long long)cr);
		tx4927_sdram_resource[i].name = "SDRAM";
		tx4927_sdram_resource[i].start = base;
		tx4927_sdram_resource[i].end = base + size - 1;
		tx4927_sdram_resource[i].flags = IORESOURCE_MEM;
		request_resource(&iomem_resource, &tx4927_sdram_resource[i]);
	}
	printk(" TR:%09llx\n",
	       (unsigned long long)____raw_readq(&tx4927_sdramcptr->tr));

	/* TMR */
	/* disable all timers */
	for (i = 0; i < TX4927_NR_TMR; i++)
		txx9_tmr_init(TX4927_TMR_REG(i) & 0xfffffffffULL);

	/* PIO */
	txx9_gpio_init(TX4927_PIO_REG & 0xfffffffffULL, 0, TX4927_NUM_PIO);
	__raw_writel(0, &tx4927_pioptr->maskcpu);
	__raw_writel(0, &tx4927_pioptr->maskext);

	_machine_restart = tx4927_machine_restart;
	board_be_init = tx4927_be_init;
}

void __init tx4927_time_init(unsigned int tmrnr)
{
	if (____raw_readq(&tx4927_ccfgptr->ccfg) & TX4927_CCFG_TINTDIS)
		txx9_clockevent_init(TX4927_TMR_REG(tmrnr) & 0xfffffffffULL,
				     TXX9_IRQ_BASE + TX4927_IR_TMR(tmrnr),
				     TXX9_IMCLK);
}

void __init tx4927_sio_init(unsigned int sclk, unsigned int cts_mask)
{
	int i;

	for (i = 0; i < 2; i++)
		txx9_sio_init(TX4927_SIO_REG(i) & 0xfffffffffULL,
			      TXX9_IRQ_BASE + TX4927_IR_SIO(i),
			      i, sclk, (1 << i) & cts_mask);
}

void __init tx4927_mtd_init(int ch)
{
	struct physmap_flash_data pdata = {
		.width = TX4927_EBUSC_WIDTH(ch) / 8,
	};
	unsigned long start = txx9_ce_res[ch].start;
	unsigned long size = txx9_ce_res[ch].end - start + 1;

	if (!(TX4927_EBUSC_CR(ch) & 0x8))
		return;	/* disabled */
	txx9_physmap_flash_init(ch, start, size, &pdata);
}

static void __init tx4927_stop_unused_modules(void)
{
	__u64 pcfg, rst = 0, ckd = 0;
	char buf[128];

	buf[0] = '\0';
	local_irq_disable();
	pcfg = ____raw_readq(&tx4927_ccfgptr->pcfg);
	if (!(pcfg & TX4927_PCFG_SEL2)) {
		rst |= TX4927_CLKCTR_ACLRST;
		ckd |= TX4927_CLKCTR_ACLCKD;
		strcat(buf, " ACLC");
	}
	if (rst | ckd) {
		txx9_set64(&tx4927_ccfgptr->clkctr, rst);
		txx9_set64(&tx4927_ccfgptr->clkctr, ckd);
	}
	local_irq_enable();
	if (buf[0])
		pr_info("%s: stop%s\n", txx9_pcode_str, buf);
}

static int __init tx4927_late_init(void)
{
	if (txx9_pcode != 0x4927)
		return -ENODEV;
	tx4927_stop_unused_modules();
	return 0;
}
late_initcall(tx4927_late_init);
