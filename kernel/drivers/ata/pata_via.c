

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/pci.h>
#include <linux/init.h>
#include <linux/blkdev.h>
#include <linux/delay.h>
#include <scsi/scsi_host.h>
#include <linux/libata.h>
#include <linux/dmi.h>

#define DRV_NAME "pata_via"
#define DRV_VERSION "0.3.3"


enum {
	VIA_UDMA	= 0x007,
	VIA_UDMA_NONE	= 0x000,
	VIA_UDMA_33	= 0x001,
	VIA_UDMA_66	= 0x002,
	VIA_UDMA_100	= 0x003,
	VIA_UDMA_133	= 0x004,
	VIA_BAD_PREQ	= 0x010, /* Crashes if PREQ# till DDACK# set */
	VIA_BAD_CLK66	= 0x020, /* 66 MHz clock doesn't work correctly */
	VIA_SET_FIFO	= 0x040, /* Needs to have FIFO split set */
	VIA_NO_UNMASK	= 0x080, /* Doesn't work with IRQ unmasking on */
	VIA_BAD_ID	= 0x100, /* Has wrong vendor ID (0x1107) */
	VIA_BAD_AST	= 0x200, /* Don't touch Address Setup Timing */
	VIA_NO_ENABLES	= 0x400, /* Has no enablebits */
	VIA_SATA_PATA	= 0x800, /* SATA/PATA combined configuration */
};

enum {
	VIA_IDFLAG_SINGLE = (1 << 0), /* single channel controller) */
};


static const struct via_isa_bridge {
	const char *name;
	u16 id;
	u8 rev_min;
	u8 rev_max;
	u16 flags;
} via_isa_bridges[] = {
	{ "vx855",	PCI_DEVICE_ID_VIA_VX855,    0x00, 0x2f,
	  VIA_UDMA_133 | VIA_BAD_AST | VIA_SATA_PATA },
	{ "vx800",	PCI_DEVICE_ID_VIA_VX800,    0x00, 0x2f, VIA_UDMA_133 |
	VIA_BAD_AST | VIA_SATA_PATA },
	{ "vt8261",	PCI_DEVICE_ID_VIA_8261,     0x00, 0x2f,
	  VIA_UDMA_133 | VIA_BAD_AST },
	{ "vt8237s",	PCI_DEVICE_ID_VIA_8237S,    0x00, 0x2f, VIA_UDMA_133 | VIA_BAD_AST },
	{ "vt8251",	PCI_DEVICE_ID_VIA_8251,     0x00, 0x2f, VIA_UDMA_133 | VIA_BAD_AST },
	{ "cx700",	PCI_DEVICE_ID_VIA_CX700,    0x00, 0x2f, VIA_UDMA_133 | VIA_BAD_AST | VIA_SATA_PATA },
	{ "vt6410",	PCI_DEVICE_ID_VIA_6410,     0x00, 0x2f, VIA_UDMA_133 | VIA_BAD_AST | VIA_NO_ENABLES },
	{ "vt6415",	PCI_DEVICE_ID_VIA_6415,     0x00, 0x2f, VIA_UDMA_133 | VIA_BAD_AST | VIA_NO_ENABLES },
	{ "vt8237a",	PCI_DEVICE_ID_VIA_8237A,    0x00, 0x2f, VIA_UDMA_133 | VIA_BAD_AST },
	{ "vt8237",	PCI_DEVICE_ID_VIA_8237,     0x00, 0x2f, VIA_UDMA_133 | VIA_BAD_AST },
	{ "vt8235",	PCI_DEVICE_ID_VIA_8235,     0x00, 0x2f, VIA_UDMA_133 | VIA_BAD_AST },
	{ "vt8233a",	PCI_DEVICE_ID_VIA_8233A,    0x00, 0x2f, VIA_UDMA_133 | VIA_BAD_AST },
	{ "vt8233c",	PCI_DEVICE_ID_VIA_8233C_0,  0x00, 0x2f, VIA_UDMA_100 },
	{ "vt8233",	PCI_DEVICE_ID_VIA_8233_0,   0x00, 0x2f, VIA_UDMA_100 },
	{ "vt8231",	PCI_DEVICE_ID_VIA_8231,     0x00, 0x2f, VIA_UDMA_100 },
	{ "vt82c686b",	PCI_DEVICE_ID_VIA_82C686,   0x40, 0x4f, VIA_UDMA_100 },
	{ "vt82c686a",	PCI_DEVICE_ID_VIA_82C686,   0x10, 0x2f, VIA_UDMA_66 },
	{ "vt82c686",	PCI_DEVICE_ID_VIA_82C686,   0x00, 0x0f, VIA_UDMA_33 | VIA_BAD_CLK66 },
	{ "vt82c596b",	PCI_DEVICE_ID_VIA_82C596,   0x10, 0x2f, VIA_UDMA_66 },
	{ "vt82c596a",	PCI_DEVICE_ID_VIA_82C596,   0x00, 0x0f, VIA_UDMA_33 | VIA_BAD_CLK66 },
	{ "vt82c586b",	PCI_DEVICE_ID_VIA_82C586_0, 0x47, 0x4f, VIA_UDMA_33 | VIA_SET_FIFO },
	{ "vt82c586b",	PCI_DEVICE_ID_VIA_82C586_0, 0x40, 0x46, VIA_UDMA_33 | VIA_SET_FIFO | VIA_BAD_PREQ },
	{ "vt82c586b",	PCI_DEVICE_ID_VIA_82C586_0, 0x30, 0x3f, VIA_UDMA_33 | VIA_SET_FIFO },
	{ "vt82c586a",	PCI_DEVICE_ID_VIA_82C586_0, 0x20, 0x2f, VIA_UDMA_33 | VIA_SET_FIFO },
	{ "vt82c586",	PCI_DEVICE_ID_VIA_82C586_0, 0x00, 0x0f, VIA_UDMA_NONE | VIA_SET_FIFO },
	{ "vt82c576",	PCI_DEVICE_ID_VIA_82C576,   0x00, 0x2f, VIA_UDMA_NONE | VIA_SET_FIFO | VIA_NO_UNMASK },
	{ "vt82c576",	PCI_DEVICE_ID_VIA_82C576,   0x00, 0x2f, VIA_UDMA_NONE | VIA_SET_FIFO | VIA_NO_UNMASK | VIA_BAD_ID },
	{ "vtxxxx",	PCI_DEVICE_ID_VIA_ANON,    0x00, 0x2f,
	  VIA_UDMA_133 | VIA_BAD_AST },
	{ NULL }
};



static const struct dmi_system_id cable_dmi_table[] = {
	{
		.ident = "Acer Ferrari 3400",
		.matches = {
			DMI_MATCH(DMI_BOARD_VENDOR, "Acer,Inc."),
			DMI_MATCH(DMI_BOARD_NAME, "Ferrari 3400"),
		},
	},
	{ }
};

static int via_cable_override(struct pci_dev *pdev)
{
	/* Systems by DMI */
	if (dmi_check_system(cable_dmi_table))
		return 1;
	/* Arima W730-K8/Targa Visionary 811/... */
	if (pdev->subsystem_vendor == 0x161F && pdev->subsystem_device == 0x2032)
		return 1;
	return 0;
}



static int via_cable_detect(struct ata_port *ap) {
	const struct via_isa_bridge *config = ap->host->private_data;
	struct pci_dev *pdev = to_pci_dev(ap->host->dev);
	u32 ata66;

	if (via_cable_override(pdev))
		return ATA_CBL_PATA40_SHORT;

	if ((config->flags & VIA_SATA_PATA) && ap->port_no == 0)
		return ATA_CBL_SATA;

	/* Early chips are 40 wire */
	if ((config->flags & VIA_UDMA) < VIA_UDMA_66)
		return ATA_CBL_PATA40;
	/* UDMA 66 chips have only drive side logic */
	else if ((config->flags & VIA_UDMA) < VIA_UDMA_100)
		return ATA_CBL_PATA_UNK;
	/* UDMA 100 or later */
	pci_read_config_dword(pdev, 0x50, &ata66);
	/* Check both the drive cable reporting bits, we might not have
	   two drives */
	if (ata66 & (0x10100000 >> (16 * ap->port_no)))
		return ATA_CBL_PATA80;
	/* Check with ACPI so we can spot BIOS reported SATA bridges */
	if (ata_acpi_init_gtm(ap) &&
	    ata_acpi_cbl_80wire(ap, ata_acpi_init_gtm(ap)))
		return ATA_CBL_PATA80;
	return ATA_CBL_PATA40;
}

static int via_pre_reset(struct ata_link *link, unsigned long deadline)
{
	struct ata_port *ap = link->ap;
	const struct via_isa_bridge *config = ap->host->private_data;

	if (!(config->flags & VIA_NO_ENABLES)) {
		static const struct pci_bits via_enable_bits[] = {
			{ 0x40, 1, 0x02, 0x02 },
			{ 0x40, 1, 0x01, 0x01 }
		};
		struct pci_dev *pdev = to_pci_dev(ap->host->dev);
		if (!pci_test_config_bits(pdev, &via_enable_bits[ap->port_no]))
			return -ENOENT;
	}

	return ata_sff_prereset(link, deadline);
}



static void via_do_set_mode(struct ata_port *ap, struct ata_device *adev, int mode, int tdiv, int set_ast, int udma_type)
{
	struct pci_dev *pdev = to_pci_dev(ap->host->dev);
	struct ata_device *peer = ata_dev_pair(adev);
	struct ata_timing t, p;
	static int via_clock = 33333;	/* Bus clock in kHZ - ought to be tunable one day */
	unsigned long T =  1000000000 / via_clock;
	unsigned long UT = T/tdiv;
	int ut;
	int offset = 3 - (2*ap->port_no) - adev->devno;

	/* Calculate the timing values we require */
	ata_timing_compute(adev, mode, &t, T, UT);

	/* We share 8bit timing so we must merge the constraints */
	if (peer) {
		if (peer->pio_mode) {
			ata_timing_compute(peer, peer->pio_mode, &p, T, UT);
			ata_timing_merge(&p, &t, &t, ATA_TIMING_8BIT);
		}
	}

	/* Address setup is programmable but breaks on UDMA133 setups */
	if (set_ast) {
		u8 setup;	/* 2 bits per drive */
		int shift = 2 * offset;

		pci_read_config_byte(pdev, 0x4C, &setup);
		setup &= ~(3 << shift);
		setup |= clamp_val(t.setup, 1, 4) << shift;	/* 1,4 or 1,4 - 1  FIXME */
		pci_write_config_byte(pdev, 0x4C, setup);
	}

	/* Load the PIO mode bits */
	pci_write_config_byte(pdev, 0x4F - ap->port_no,
		((clamp_val(t.act8b, 1, 16) - 1) << 4) | (clamp_val(t.rec8b, 1, 16) - 1));
	pci_write_config_byte(pdev, 0x48 + offset,
		((clamp_val(t.active, 1, 16) - 1) << 4) | (clamp_val(t.recover, 1, 16) - 1));

	/* Load the UDMA bits according to type */
	switch(udma_type) {
		default:
			/* BUG() ? */
			/* fall through */
		case 33:
			ut = t.udma ? (0xe0 | (clamp_val(t.udma, 2, 5) - 2)) : 0x03;
			break;
		case 66:
			ut = t.udma ? (0xe8 | (clamp_val(t.udma, 2, 9) - 2)) : 0x0f;
			break;
		case 100:
			ut = t.udma ? (0xe0 | (clamp_val(t.udma, 2, 9) - 2)) : 0x07;
			break;
		case 133:
			ut = t.udma ? (0xe0 | (clamp_val(t.udma, 2, 9) - 2)) : 0x07;
			break;
	}

	/* Set UDMA unless device is not UDMA capable */
	if (udma_type && t.udma) {
		u8 cable80_status;

		/* Get 80-wire cable detection bit */
		pci_read_config_byte(pdev, 0x50 + offset, &cable80_status);
		cable80_status &= 0x10;

		pci_write_config_byte(pdev, 0x50 + offset, ut | cable80_status);
	}
}

static void via_set_piomode(struct ata_port *ap, struct ata_device *adev)
{
	const struct via_isa_bridge *config = ap->host->private_data;
	int set_ast = (config->flags & VIA_BAD_AST) ? 0 : 1;
	int mode = config->flags & VIA_UDMA;
	static u8 tclock[5] = { 1, 1, 2, 3, 4 };
	static u8 udma[5] = { 0, 33, 66, 100, 133 };

	via_do_set_mode(ap, adev, adev->pio_mode, tclock[mode], set_ast, udma[mode]);
}

static void via_set_dmamode(struct ata_port *ap, struct ata_device *adev)
{
	const struct via_isa_bridge *config = ap->host->private_data;
	int set_ast = (config->flags & VIA_BAD_AST) ? 0 : 1;
	int mode = config->flags & VIA_UDMA;
	static u8 tclock[5] = { 1, 1, 2, 3, 4 };
	static u8 udma[5] = { 0, 33, 66, 100, 133 };

	via_do_set_mode(ap, adev, adev->dma_mode, tclock[mode], set_ast, udma[mode]);
}

static void via_tf_load(struct ata_port *ap, const struct ata_taskfile *tf)
{
	struct ata_taskfile tmp_tf;

	if (ap->ctl != ap->last_ctl && !(tf->flags & ATA_TFLAG_DEVICE)) {
		tmp_tf = *tf;
		tmp_tf.flags |= ATA_TFLAG_DEVICE;
		tf = &tmp_tf;
	}
	ata_sff_tf_load(ap, tf);
}

static struct scsi_host_template via_sht = {
	ATA_BMDMA_SHT(DRV_NAME),
};

static struct ata_port_operations via_port_ops = {
	.inherits	= &ata_bmdma_port_ops,
	.cable_detect	= via_cable_detect,
	.set_piomode	= via_set_piomode,
	.set_dmamode	= via_set_dmamode,
	.prereset	= via_pre_reset,
	.sff_tf_load	= via_tf_load,
};

static struct ata_port_operations via_port_ops_noirq = {
	.inherits	= &via_port_ops,
	.sff_data_xfer	= ata_sff_data_xfer_noirq,
};


static void via_config_fifo(struct pci_dev *pdev, unsigned int flags)
{
	u8 enable;

	/* 0x40 low bits indicate enabled channels */
	pci_read_config_byte(pdev, 0x40 , &enable);
	enable &= 3;

	if (flags & VIA_SET_FIFO) {
		static const u8 fifo_setting[4] = {0x00, 0x60, 0x00, 0x20};
		u8 fifo;

		pci_read_config_byte(pdev, 0x43, &fifo);

		/* Clear PREQ# until DDACK# for errata */
		if (flags & VIA_BAD_PREQ)
			fifo &= 0x7F;
		else
			fifo &= 0x9f;
		/* Turn on FIFO for enabled channels */
		fifo |= fifo_setting[enable];
		pci_write_config_byte(pdev, 0x43, fifo);
	}
}


static int via_init_one(struct pci_dev *pdev, const struct pci_device_id *id)
{
	/* Early VIA without UDMA support */
	static const struct ata_port_info via_mwdma_info = {
		.flags = ATA_FLAG_SLAVE_POSS,
		.pio_mask = 0x1f,
		.mwdma_mask = 0x07,
		.port_ops = &via_port_ops
	};
	/* Ditto with IRQ masking required */
	static const struct ata_port_info via_mwdma_info_borked = {
		.flags = ATA_FLAG_SLAVE_POSS,
		.pio_mask = 0x1f,
		.mwdma_mask = 0x07,
		.port_ops = &via_port_ops_noirq,
	};
	/* VIA UDMA 33 devices (and borked 66) */
	static const struct ata_port_info via_udma33_info = {
		.flags = ATA_FLAG_SLAVE_POSS,
		.pio_mask = 0x1f,
		.mwdma_mask = 0x07,
		.udma_mask = ATA_UDMA2,
		.port_ops = &via_port_ops
	};
	/* VIA UDMA 66 devices */
	static const struct ata_port_info via_udma66_info = {
		.flags = ATA_FLAG_SLAVE_POSS,
		.pio_mask = 0x1f,
		.mwdma_mask = 0x07,
		.udma_mask = ATA_UDMA4,
		.port_ops = &via_port_ops
	};
	/* VIA UDMA 100 devices */
	static const struct ata_port_info via_udma100_info = {
		.flags = ATA_FLAG_SLAVE_POSS,
		.pio_mask = 0x1f,
		.mwdma_mask = 0x07,
		.udma_mask = ATA_UDMA5,
		.port_ops = &via_port_ops
	};
	/* UDMA133 with bad AST (All current 133) */
	static const struct ata_port_info via_udma133_info = {
		.flags = ATA_FLAG_SLAVE_POSS,
		.pio_mask = 0x1f,
		.mwdma_mask = 0x07,
		.udma_mask = ATA_UDMA6,	/* FIXME: should check north bridge */
		.port_ops = &via_port_ops
	};
	const struct ata_port_info *ppi[] = { NULL, NULL };
	struct pci_dev *isa = NULL;
	const struct via_isa_bridge *config;
	static int printed_version;
	u8 enable;
	u32 timing;
	unsigned long flags = id->driver_data;
	int rc;

	if (!printed_version++)
		dev_printk(KERN_DEBUG, &pdev->dev, "version " DRV_VERSION "\n");

	rc = pcim_enable_device(pdev);
	if (rc)
		return rc;

	if (flags & VIA_IDFLAG_SINGLE)
		ppi[1] = &ata_dummy_port_info;

	/* To find out how the IDE will behave and what features we
	   actually have to look at the bridge not the IDE controller */
	for (config = via_isa_bridges; config->id != PCI_DEVICE_ID_VIA_ANON;
	     config++)
		if ((isa = pci_get_device(PCI_VENDOR_ID_VIA +
			!!(config->flags & VIA_BAD_ID),
			config->id, NULL))) {

			if (isa->revision >= config->rev_min &&
			    isa->revision <= config->rev_max)
				break;
			pci_dev_put(isa);
		}

	pci_dev_put(isa);

	if (!(config->flags & VIA_NO_ENABLES)) {
		/* 0x40 low bits indicate enabled channels */
		pci_read_config_byte(pdev, 0x40 , &enable);
		enable &= 3;
		if (enable == 0)
			return -ENODEV;
	}

	/* Initialise the FIFO for the enabled channels. */
	via_config_fifo(pdev, config->flags);

	/* Clock set up */
	switch(config->flags & VIA_UDMA) {
		case VIA_UDMA_NONE:
			if (config->flags & VIA_NO_UNMASK)
				ppi[0] = &via_mwdma_info_borked;
			else
				ppi[0] = &via_mwdma_info;
			break;
		case VIA_UDMA_33:
			ppi[0] = &via_udma33_info;
			break;
		case VIA_UDMA_66:
			ppi[0] = &via_udma66_info;
			/* The 66 MHz devices require we enable the clock */
			pci_read_config_dword(pdev, 0x50, &timing);
			timing |= 0x80008;
			pci_write_config_dword(pdev, 0x50, timing);
			break;
		case VIA_UDMA_100:
			ppi[0] = &via_udma100_info;
			break;
		case VIA_UDMA_133:
			ppi[0] = &via_udma133_info;
			break;
		default:
			WARN_ON(1);
			return -ENODEV;
	}

	if (config->flags & VIA_BAD_CLK66) {
		/* Disable the 66MHz clock on problem devices */
		pci_read_config_dword(pdev, 0x50, &timing);
		timing &= ~0x80008;
		pci_write_config_dword(pdev, 0x50, timing);
	}

	/* We have established the device type, now fire it up */
	return ata_pci_sff_init_one(pdev, ppi, &via_sht, (void *)config);
}

#ifdef CONFIG_PM

static int via_reinit_one(struct pci_dev *pdev)
{
	u32 timing;
	struct ata_host *host = dev_get_drvdata(&pdev->dev);
	const struct via_isa_bridge *config = host->private_data;
	int rc;

	rc = ata_pci_device_do_resume(pdev);
	if (rc)
		return rc;

	via_config_fifo(pdev, config->flags);

	if ((config->flags & VIA_UDMA) == VIA_UDMA_66) {
		/* The 66 MHz devices require we enable the clock */
		pci_read_config_dword(pdev, 0x50, &timing);
		timing |= 0x80008;
		pci_write_config_dword(pdev, 0x50, timing);
	}
	if (config->flags & VIA_BAD_CLK66) {
		/* Disable the 66MHz clock on problem devices */
		pci_read_config_dword(pdev, 0x50, &timing);
		timing &= ~0x80008;
		pci_write_config_dword(pdev, 0x50, timing);
	}

	ata_host_resume(host);
	return 0;
}
#endif

static const struct pci_device_id via[] = {
	{ PCI_VDEVICE(VIA, 0x0415), },
	{ PCI_VDEVICE(VIA, 0x0571), },
	{ PCI_VDEVICE(VIA, 0x0581), },
	{ PCI_VDEVICE(VIA, 0x1571), },
	{ PCI_VDEVICE(VIA, 0x3164), },
	{ PCI_VDEVICE(VIA, 0x5324), },
	{ PCI_VDEVICE(VIA, 0xC409), VIA_IDFLAG_SINGLE },

	{ },
};

static struct pci_driver via_pci_driver = {
	.name 		= DRV_NAME,
	.id_table	= via,
	.probe 		= via_init_one,
	.remove		= ata_pci_remove_one,
#ifdef CONFIG_PM
	.suspend	= ata_pci_device_suspend,
	.resume		= via_reinit_one,
#endif
};

static int __init via_init(void)
{
	return pci_register_driver(&via_pci_driver);
}

static void __exit via_exit(void)
{
	pci_unregister_driver(&via_pci_driver);
}

MODULE_AUTHOR("Alan Cox");
MODULE_DESCRIPTION("low-level driver for VIA PATA");
MODULE_LICENSE("GPL");
MODULE_DEVICE_TABLE(pci, via);
MODULE_VERSION(DRV_VERSION);

module_init(via_init);
module_exit(via_exit);
