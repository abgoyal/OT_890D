

#include <linux/module.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/fs.h>
#include <linux/root_dev.h>
#include <linux/mtd/mtd.h>
#include <linux/mtd/map.h>

#include <asm/io.h>

#include <msp_prom.h>

static struct mtd_info *rr_mtd;

struct map_info rr_map = {
	.name = "ramroot",
	.bankwidth = 4,
};

static int __init init_rrmap(void)
{
	void *ramroot_start;
	unsigned long ramroot_size;

	/* Check for supported rootfs types */
	if (get_ramroot(&ramroot_start, &ramroot_size)) {
		rr_map.phys = CPHYSADDR(ramroot_start);
		rr_map.size = ramroot_size;

		printk(KERN_NOTICE
			"PMC embedded root device: 0x%08lx @ 0x%08lx\n",
			rr_map.size, (unsigned long)rr_map.phys);
	} else {
		printk(KERN_ERR
			"init_rrmap: no supported embedded rootfs detected!\n");
		return -ENXIO;
	}

	/* Map rootfs to I/O space for block device driver */
	rr_map.virt = ioremap(rr_map.phys, rr_map.size);
	if (!rr_map.virt) {
		printk(KERN_ERR "Failed to ioremap\n");
		return -EIO;
	}

	simple_map_init(&rr_map);

	rr_mtd = do_map_probe("map_ram", &rr_map);
	if (rr_mtd) {
		rr_mtd->owner = THIS_MODULE;

		add_mtd_device(rr_mtd);

		return 0;
	}

	iounmap(rr_map.virt);
	return -ENXIO;
}

static void __exit cleanup_rrmap(void)
{
	del_mtd_device(rr_mtd);
	map_destroy(rr_mtd);

	iounmap(rr_map.virt);
	rr_map.virt = NULL;
}

MODULE_AUTHOR("PMC-Sierra, Inc");
MODULE_DESCRIPTION("MTD map driver for embedded PMC-Sierra MSP filesystem");
MODULE_LICENSE("GPL");

module_init(init_rrmap);
module_exit(cleanup_rrmap);
