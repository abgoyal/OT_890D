


#define DRV_NAME	"ht6560b"
#define HT6560B_VERSION "v0.08"

#include <linux/module.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/timer.h>
#include <linux/mm.h>
#include <linux/ioport.h>
#include <linux/blkdev.h>
#include <linux/ide.h>
#include <linux/init.h>

#include <asm/io.h>

/* #define DEBUG */  /* remove comments for DEBUG messages */

#define HT_CONFIG_PORT	  0x3e6
#define HT_CONFIG(drivea) (u8)(((drivea)->drive_data & 0xff00) >> 8)
#define HT_CONFIG_DEFAULT 0x1c /* no prefetch */
/* #define HT_CONFIG_DEFAULT 0x3c */ /* with prefetch */
#define HT_SECONDARY_IF	  0x01
#define HT_PREFETCH_MODE  0x20

#define HT_TIMING(drivea) (u8)((drivea)->drive_data & 0x00ff)
#define HT_TIMING_DEFAULT 0xff


static void ht6560b_selectproc (ide_drive_t *drive)
{
	ide_hwif_t *hwif = drive->hwif;
	unsigned long flags;
	static u8 current_select = 0;
	static u8 current_timing = 0;
	u8 select, timing;
	
	local_irq_save(flags);

	select = HT_CONFIG(drive);
	timing = HT_TIMING(drive);

	/*
	 * Need to enforce prefetch sometimes because otherwise
	 * it'll hang (hard).
	 */
	if (drive->media != ide_disk ||
	    (drive->dev_flags & IDE_DFLAG_PRESENT) == 0)
		select |= HT_PREFETCH_MODE;

	if (select != current_select || timing != current_timing) {
		current_select = select;
		current_timing = timing;
		(void)inb(HT_CONFIG_PORT);
		(void)inb(HT_CONFIG_PORT);
		(void)inb(HT_CONFIG_PORT);
		(void)inb(HT_CONFIG_PORT);
		outb(select, HT_CONFIG_PORT);
		/*
		 * Set timing for this drive:
		 */
		outb(timing, hwif->io_ports.device_addr);
		(void)inb(hwif->io_ports.status_addr);
#ifdef DEBUG
		printk("ht6560b: %s: select=%#x timing=%#x\n",
			drive->name, select, timing);
#endif
	}
	local_irq_restore(flags);
}

static int __init try_to_init_ht6560b(void)
{
	u8 orig_value;
	int i;
	
	/* Autodetect ht6560b */
	if ((orig_value = inb(HT_CONFIG_PORT)) == 0xff)
		return 0;
	
	for (i=3;i>0;i--) {
		outb(0x00, HT_CONFIG_PORT);
		if (!( (~inb(HT_CONFIG_PORT)) & 0x3f )) {
			outb(orig_value, HT_CONFIG_PORT);
			return 0;
		}
	}
	outb(0x00, HT_CONFIG_PORT);
	if ((~inb(HT_CONFIG_PORT))& 0x3f) {
		outb(orig_value, HT_CONFIG_PORT);
		return 0;
	}
	/*
	 * Ht6560b autodetected
	 */
	outb(HT_CONFIG_DEFAULT, HT_CONFIG_PORT);
	outb(HT_TIMING_DEFAULT, 0x1f6);	/* Select register */
	(void)inb(0x1f7);		/* Status register */

	printk("ht6560b " HT6560B_VERSION
	       ": chipset detected and initialized"
#ifdef DEBUG
	       " with debug enabled"
#endif
	       "\n"
		);
	return 1;
}

static u8 ht_pio2timings(ide_drive_t *drive, const u8 pio)
{
	int active_time, recovery_time;
	int active_cycles, recovery_cycles;
	int bus_speed = ide_vlb_clk ? ide_vlb_clk : 50;

        if (pio) {
		unsigned int cycle_time;
		struct ide_timing *t = ide_timing_find_mode(XFER_PIO_0 + pio);

		cycle_time = ide_pio_cycle_time(drive, pio);

		/*
		 *  Just like opti621.c we try to calculate the
		 *  actual cycle time for recovery and activity
		 *  according system bus speed.
		 */
		active_time = t->active;
		recovery_time = cycle_time - active_time - t->setup;
		/*
		 *  Cycle times should be Vesa bus cycles
		 */
		active_cycles   = (active_time   * bus_speed + 999) / 1000;
		recovery_cycles = (recovery_time * bus_speed + 999) / 1000;
		/*
		 *  Upper and lower limits
		 */
		if (active_cycles   < 2)  active_cycles   = 2;
		if (recovery_cycles < 2)  recovery_cycles = 2;
		if (active_cycles   > 15) active_cycles   = 15;
		if (recovery_cycles > 15) recovery_cycles = 0;  /* 0==16 */
		
#ifdef DEBUG
		printk("ht6560b: drive %s setting pio=%d recovery=%d (%dns) active=%d (%dns)\n", drive->name, pio, recovery_cycles, recovery_time, active_cycles, active_time);
#endif
		
		return (u8)((recovery_cycles << 4) | active_cycles);
	} else {
		
#ifdef DEBUG
		printk("ht6560b: drive %s setting pio=0\n", drive->name);
#endif
		
		return HT_TIMING_DEFAULT;    /* default setting */
	}
}

static DEFINE_SPINLOCK(ht6560b_lock);

static void ht_set_prefetch(ide_drive_t *drive, u8 state)
{
	unsigned long flags;
	int t = HT_PREFETCH_MODE << 8;

	spin_lock_irqsave(&ht6560b_lock, flags);

	/*
	 *  Prefetch mode and unmask irq seems to conflict
	 */
	if (state) {
		drive->drive_data |= t;   /* enable prefetch mode */
		drive->dev_flags |= IDE_DFLAG_NO_UNMASK;
		drive->dev_flags &= ~IDE_DFLAG_UNMASK;
	} else {
		drive->drive_data &= ~t;  /* disable prefetch mode */
		drive->dev_flags &= ~IDE_DFLAG_NO_UNMASK;
	}

	spin_unlock_irqrestore(&ht6560b_lock, flags);

#ifdef DEBUG
	printk("ht6560b: drive %s prefetch mode %sabled\n", drive->name, (state ? "en" : "dis"));
#endif
}

static void ht6560b_set_pio_mode(ide_drive_t *drive, const u8 pio)
{
	unsigned long flags;
	u8 timing;
	
	switch (pio) {
	case 8:         /* set prefetch off */
	case 9:         /* set prefetch on */
		ht_set_prefetch(drive, pio & 1);
		return;
	}

	timing = ht_pio2timings(drive, pio);

	spin_lock_irqsave(&ht6560b_lock, flags);
	drive->drive_data &= 0xff00;
	drive->drive_data |= timing;
	spin_unlock_irqrestore(&ht6560b_lock, flags);

#ifdef DEBUG
	printk("ht6560b: drive %s tuned to pio mode %#x timing=%#x\n", drive->name, pio, timing);
#endif
}

static void __init ht6560b_init_dev(ide_drive_t *drive)
{
	ide_hwif_t *hwif = drive->hwif;
	/* Setting default configurations for drives. */
	int t = (HT_CONFIG_DEFAULT << 8) | HT_TIMING_DEFAULT;

	if (hwif->channel)
		t |= (HT_SECONDARY_IF << 8);

	drive->drive_data = t;
}

static int probe_ht6560b;

module_param_named(probe, probe_ht6560b, bool, 0);
MODULE_PARM_DESC(probe, "probe for HT6560B chipset");

static const struct ide_port_ops ht6560b_port_ops = {
	.init_dev		= ht6560b_init_dev,
	.set_pio_mode		= ht6560b_set_pio_mode,
	.selectproc		= ht6560b_selectproc,
};

static const struct ide_port_info ht6560b_port_info __initdata = {
	.name			= DRV_NAME,
	.chipset		= ide_ht6560b,
	.port_ops		= &ht6560b_port_ops,
	.host_flags		= IDE_HFLAG_SERIALIZE | /* is this needed? */
				  IDE_HFLAG_NO_DMA |
				  IDE_HFLAG_ABUSE_PREFETCH,
	.pio_mask		= ATA_PIO4,
};

static int __init ht6560b_init(void)
{
	if (probe_ht6560b == 0)
		return -ENODEV;

	if (!request_region(HT_CONFIG_PORT, 1, DRV_NAME)) {
		printk(KERN_NOTICE "%s: HT_CONFIG_PORT not found\n",
			__func__);
		return -ENODEV;
	}

	if (!try_to_init_ht6560b()) {
		printk(KERN_NOTICE "%s: HBA not found\n", __func__);
		goto release_region;
	}

	return ide_legacy_device_add(&ht6560b_port_info, 0);

release_region:
	release_region(HT_CONFIG_PORT, 1);
	return -ENODEV;
}

module_init(ht6560b_init);

MODULE_AUTHOR("See Local File");
MODULE_DESCRIPTION("HT-6560B EIDE-controller support");
MODULE_LICENSE("GPL");
