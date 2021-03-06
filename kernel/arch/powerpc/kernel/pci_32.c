

#include <linux/kernel.h>
#include <linux/pci.h>
#include <linux/delay.h>
#include <linux/string.h>
#include <linux/init.h>
#include <linux/capability.h>
#include <linux/sched.h>
#include <linux/errno.h>
#include <linux/bootmem.h>
#include <linux/irq.h>
#include <linux/list.h>
#include <linux/of.h>

#include <asm/processor.h>
#include <asm/io.h>
#include <asm/prom.h>
#include <asm/sections.h>
#include <asm/pci-bridge.h>
#include <asm/byteorder.h>
#include <asm/uaccess.h>
#include <asm/machdep.h>

#undef DEBUG

unsigned long isa_io_base     = 0;
unsigned long pci_dram_offset = 0;
int pcibios_assign_bus_offset = 1;

void pcibios_make_OF_bus_map(void);

static void fixup_broken_pcnet32(struct pci_dev* dev);
static void fixup_cpc710_pci64(struct pci_dev* dev);
#ifdef CONFIG_PPC_OF
static u8* pci_to_OF_bus_map;
#endif

static int pci_assign_all_buses;

LIST_HEAD(hose_list);

static int pci_bus_count;

struct pci_dev *isa_bridge_pcidev;
EXPORT_SYMBOL_GPL(isa_bridge_pcidev);

static void
fixup_hide_host_resource_fsl(struct pci_dev *dev)
{
	int i, class = dev->class >> 8;

	if ((class == PCI_CLASS_PROCESSOR_POWERPC ||
	     class == PCI_CLASS_BRIDGE_OTHER) &&
		(dev->hdr_type == PCI_HEADER_TYPE_NORMAL) &&
		(dev->bus->parent == NULL)) {
		for (i = 0; i < DEVICE_COUNT_RESOURCE; i++) {
			dev->resource[i].start = 0;
			dev->resource[i].end = 0;
			dev->resource[i].flags = 0;
		}
	}
}
DECLARE_PCI_FIXUP_HEADER(PCI_VENDOR_ID_MOTOROLA, PCI_ANY_ID, fixup_hide_host_resource_fsl); 
DECLARE_PCI_FIXUP_HEADER(PCI_VENDOR_ID_FREESCALE, PCI_ANY_ID, fixup_hide_host_resource_fsl); 

static void
fixup_broken_pcnet32(struct pci_dev* dev)
{
	if ((dev->class>>8 == PCI_CLASS_NETWORK_ETHERNET)) {
		dev->vendor = PCI_VENDOR_ID_AMD;
		pci_write_config_word(dev, PCI_VENDOR_ID, PCI_VENDOR_ID_AMD);
	}
}
DECLARE_PCI_FIXUP_HEADER(PCI_VENDOR_ID_TRIDENT,	PCI_ANY_ID,			fixup_broken_pcnet32);

static void
fixup_cpc710_pci64(struct pci_dev* dev)
{
	/* Hide the PCI64 BARs from the kernel as their content doesn't
	 * fit well in the resource management
	 */
	dev->resource[0].start = dev->resource[0].end = 0;
	dev->resource[0].flags = 0;
	dev->resource[1].start = dev->resource[1].end = 0;
	dev->resource[1].flags = 0;
}
DECLARE_PCI_FIXUP_HEADER(PCI_VENDOR_ID_IBM,	PCI_DEVICE_ID_IBM_CPC710_PCI64,	fixup_cpc710_pci64);

#ifdef CONFIG_PPC_OF
static void
make_one_node_map(struct device_node* node, u8 pci_bus)
{
	const int *bus_range;
	int len;

	if (pci_bus >= pci_bus_count)
		return;
	bus_range = of_get_property(node, "bus-range", &len);
	if (bus_range == NULL || len < 2 * sizeof(int)) {
		printk(KERN_WARNING "Can't get bus-range for %s, "
		       "assuming it starts at 0\n", node->full_name);
		pci_to_OF_bus_map[pci_bus] = 0;
	} else
		pci_to_OF_bus_map[pci_bus] = bus_range[0];

	for_each_child_of_node(node, node) {
		struct pci_dev* dev;
		const unsigned int *class_code, *reg;
	
		class_code = of_get_property(node, "class-code", NULL);
		if (!class_code || ((*class_code >> 8) != PCI_CLASS_BRIDGE_PCI &&
			(*class_code >> 8) != PCI_CLASS_BRIDGE_CARDBUS))
			continue;
		reg = of_get_property(node, "reg", NULL);
		if (!reg)
			continue;
		dev = pci_get_bus_and_slot(pci_bus, ((reg[0] >> 8) & 0xff));
		if (!dev || !dev->subordinate) {
			pci_dev_put(dev);
			continue;
		}
		make_one_node_map(node, dev->subordinate->number);
		pci_dev_put(dev);
	}
}
	
void
pcibios_make_OF_bus_map(void)
{
	int i;
	struct pci_controller *hose, *tmp;
	struct property *map_prop;
	struct device_node *dn;

	pci_to_OF_bus_map = kmalloc(pci_bus_count, GFP_KERNEL);
	if (!pci_to_OF_bus_map) {
		printk(KERN_ERR "Can't allocate OF bus map !\n");
		return;
	}

	/* We fill the bus map with invalid values, that helps
	 * debugging.
	 */
	for (i=0; i<pci_bus_count; i++)
		pci_to_OF_bus_map[i] = 0xff;

	/* For each hose, we begin searching bridges */
	list_for_each_entry_safe(hose, tmp, &hose_list, list_node) {
		struct device_node* node = hose->dn;

		if (!node)
			continue;
		make_one_node_map(node, hose->first_busno);
	}
	dn = of_find_node_by_path("/");
	map_prop = of_find_property(dn, "pci-OF-bus-map", NULL);
	if (map_prop) {
		BUG_ON(pci_bus_count > map_prop->length);
		memcpy(map_prop->value, pci_to_OF_bus_map, pci_bus_count);
	}
	of_node_put(dn);
#ifdef DEBUG
	printk("PCI->OF bus map:\n");
	for (i=0; i<pci_bus_count; i++) {
		if (pci_to_OF_bus_map[i] == 0xff)
			continue;
		printk("%d -> %d\n", i, pci_to_OF_bus_map[i]);
	}
#endif
}

typedef int (*pci_OF_scan_iterator)(struct device_node* node, void* data);

static struct device_node*
scan_OF_pci_childs(struct device_node *parent, pci_OF_scan_iterator filter, void* data)
{
	struct device_node *node;
	struct device_node* sub_node;

	for_each_child_of_node(parent, node) {
		const unsigned int *class_code;
	
		if (filter(node, data)) {
			of_node_put(node);
			return node;
		}

		/* For PCI<->PCI bridges or CardBus bridges, we go down
		 * Note: some OFs create a parent node "multifunc-device" as
		 * a fake root for all functions of a multi-function device,
		 * we go down them as well.
		 */
		class_code = of_get_property(node, "class-code", NULL);
		if ((!class_code || ((*class_code >> 8) != PCI_CLASS_BRIDGE_PCI &&
			(*class_code >> 8) != PCI_CLASS_BRIDGE_CARDBUS)) &&
			strcmp(node->name, "multifunc-device"))
			continue;
		sub_node = scan_OF_pci_childs(node, filter, data);
		if (sub_node) {
			of_node_put(node);
			return sub_node;
		}
	}
	return NULL;
}

static struct device_node *scan_OF_for_pci_dev(struct device_node *parent,
					       unsigned int devfn)
{
	struct device_node *np;
	const u32 *reg;
	unsigned int psize;

	for_each_child_of_node(parent, np) {
		reg = of_get_property(np, "reg", &psize);
		if (reg == NULL || psize < 4)
			continue;
		if (((reg[0] >> 8) & 0xff) == devfn)
			return np;
	}
	return NULL;
}


static struct device_node *scan_OF_for_pci_bus(struct pci_bus *bus)
{
	struct device_node *parent, *np;

	/* Are we a root bus ? */
	if (bus->self == NULL || bus->parent == NULL) {
		struct pci_controller *hose = pci_bus_to_host(bus);
		if (hose == NULL)
			return NULL;
		return of_node_get(hose->dn);
	}

	/* not a root bus, we need to get our parent */
	parent = scan_OF_for_pci_bus(bus->parent);
	if (parent == NULL)
		return NULL;

	/* now iterate for children for a match */
	np = scan_OF_for_pci_dev(parent, bus->self->devfn);
	of_node_put(parent);

	return np;
}

struct device_node *
pci_busdev_to_OF_node(struct pci_bus *bus, int devfn)
{
	struct device_node *parent, *np;

	pr_debug("pci_busdev_to_OF_node(%d,0x%x)\n", bus->number, devfn);
	parent = scan_OF_for_pci_bus(bus);
	if (parent == NULL)
		return NULL;
	pr_debug(" parent is %s\n", parent ? parent->full_name : "<NULL>");
	np = scan_OF_for_pci_dev(parent, devfn);
	of_node_put(parent);
	pr_debug(" result is %s\n", np ? np->full_name : "<NULL>");

	/* XXX most callers don't release the returned node
	 * mostly because ppc64 doesn't increase the refcount,
	 * we need to fix that.
	 */
	return np;
}
EXPORT_SYMBOL(pci_busdev_to_OF_node);

struct device_node*
pci_device_to_OF_node(struct pci_dev *dev)
{
	return pci_busdev_to_OF_node(dev->bus, dev->devfn);
}
EXPORT_SYMBOL(pci_device_to_OF_node);

static int
find_OF_pci_device_filter(struct device_node* node, void* data)
{
	return ((void *)node == data);
}

int
pci_device_from_OF_node(struct device_node* node, u8* bus, u8* devfn)
{
	const unsigned int *reg;
	struct pci_controller* hose;
	struct pci_dev* dev = NULL;
	
	/* Make sure it's really a PCI device */
	hose = pci_find_hose_for_OF_device(node);
	if (!hose || !hose->dn)
		return -ENODEV;
	if (!scan_OF_pci_childs(hose->dn,
			find_OF_pci_device_filter, (void *)node))
		return -ENODEV;
	reg = of_get_property(node, "reg", NULL);
	if (!reg)
		return -ENODEV;
	*bus = (reg[0] >> 16) & 0xff;
	*devfn = ((reg[0] >> 8) & 0xff);

	/* Ok, here we need some tweak. If we have already renumbered
	 * all busses, we can't rely on the OF bus number any more.
	 * the pci_to_OF_bus_map is not enough as several PCI busses
	 * may match the same OF bus number.
	 */
	if (!pci_to_OF_bus_map)
		return 0;

	for_each_pci_dev(dev)
		if (pci_to_OF_bus_map[dev->bus->number] == *bus &&
				dev->devfn == *devfn) {
			*bus = dev->bus->number;
			pci_dev_put(dev);
			return 0;
		}

	return -ENODEV;
}
EXPORT_SYMBOL(pci_device_from_OF_node);

void __init
pci_create_OF_bus_map(void)
{
	struct property* of_prop;
	struct device_node *dn;

	of_prop = (struct property*) alloc_bootmem(sizeof(struct property) + 256);
	if (!of_prop)
		return;
	dn = of_find_node_by_path("/");
	if (dn) {
		memset(of_prop, -1, sizeof(struct property) + 256);
		of_prop->name = "pci-OF-bus-map";
		of_prop->length = 256;
		of_prop->value = &of_prop[1];
		prom_add_property(dn, of_prop);
		of_node_put(dn);
	}
}

#else /* CONFIG_PPC_OF */
void pcibios_make_OF_bus_map(void)
{
}
#endif /* CONFIG_PPC_OF */

static void __devinit pcibios_scan_phb(struct pci_controller *hose)
{
	struct pci_bus *bus;
	struct device_node *node = hose->dn;
	unsigned long io_offset;
	struct resource *res = &hose->io_resource;

	pr_debug("PCI: Scanning PHB %s\n",
		 node ? node->full_name : "<NO NAME>");

	/* Create an empty bus for the toplevel */
	bus = pci_create_bus(hose->parent, hose->first_busno, hose->ops, hose);
	if (bus == NULL) {
		printk(KERN_ERR "Failed to create bus for PCI domain %04x\n",
		       hose->global_number);
		return;
	}
	bus->secondary = hose->first_busno;
	hose->bus = bus;

	/* Fixup IO space offset */
	io_offset = (unsigned long)hose->io_base_virt - isa_io_base;
	res->start = (res->start + io_offset) & 0xffffffffu;
	res->end = (res->end + io_offset) & 0xffffffffu;

	/* Wire up PHB bus resources */
	pcibios_setup_phb_resources(hose);

	/* Scan children */
	hose->last_busno = bus->subordinate = pci_scan_child_bus(bus);
}

static int __init pcibios_init(void)
{
	struct pci_controller *hose, *tmp;
	int next_busno = 0;

	printk(KERN_INFO "PCI: Probing PCI hardware\n");

	if (ppc_pci_flags & PPC_PCI_REASSIGN_ALL_BUS)
		pci_assign_all_buses = 1;

	/* Scan all of the recorded PCI controllers.  */
	list_for_each_entry_safe(hose, tmp, &hose_list, list_node) {
		if (pci_assign_all_buses)
			hose->first_busno = next_busno;
		hose->last_busno = 0xff;
		pcibios_scan_phb(hose);
		pci_bus_add_devices(hose->bus);
		if (pci_assign_all_buses || next_busno <= hose->last_busno)
			next_busno = hose->last_busno + pcibios_assign_bus_offset;
	}
	pci_bus_count = next_busno;

	/* OpenFirmware based machines need a map of OF bus
	 * numbers vs. kernel bus numbers since we may have to
	 * remap them.
	 */
	if (pci_assign_all_buses)
		pcibios_make_OF_bus_map();

	/* Call common code to handle resource allocation */
	pcibios_resource_survey();

	/* Call machine dependent post-init code */
	if (ppc_md.pcibios_after_init)
		ppc_md.pcibios_after_init();

	return 0;
}

subsys_initcall(pcibios_init);

/* the next one is stolen from the alpha port... */
void __init
pcibios_update_irq(struct pci_dev *dev, int irq)
{
	pci_write_config_byte(dev, PCI_INTERRUPT_LINE, irq);
	/* XXX FIXME - update OF device tree node interrupt property */
}

static struct pci_controller*
pci_bus_to_hose(int bus)
{
	struct pci_controller *hose, *tmp;

	list_for_each_entry_safe(hose, tmp, &hose_list, list_node)
		if (bus >= hose->first_busno && bus <= hose->last_busno)
			return hose;
	return NULL;
}


long sys_pciconfig_iobase(long which, unsigned long bus, unsigned long devfn)
{
	struct pci_controller* hose;
	long result = -EOPNOTSUPP;

	hose = pci_bus_to_hose(bus);
	if (!hose)
		return -ENODEV;

	switch (which) {
	case IOBASE_BRIDGE_NUMBER:
		return (long)hose->first_busno;
	case IOBASE_MEMORY:
		return (long)hose->pci_mem_offset;
	case IOBASE_IO:
		return (long)hose->io_base_phys;
	case IOBASE_ISA_IO:
		return (long)isa_io_base;
	case IOBASE_ISA_MEM:
		return (long)isa_mem_base;
	}

	return result;
}

unsigned long pci_address_to_pio(phys_addr_t address)
{
	struct pci_controller *hose, *tmp;

	list_for_each_entry_safe(hose, tmp, &hose_list, list_node) {
		unsigned int size = hose->io_resource.end -
			hose->io_resource.start + 1;
		if (address >= hose->io_base_phys &&
		    address < (hose->io_base_phys + size)) {
			unsigned long base =
				(unsigned long)hose->io_base_virt - _IO_BASE;
			return base + (address - hose->io_base_phys);
		}
	}
	return (unsigned int)-1;
}
EXPORT_SYMBOL(pci_address_to_pio);

#define NULL_PCI_OP(rw, size, type)					\
static int								\
null_##rw##_config_##size(struct pci_dev *dev, int offset, type val)	\
{									\
	return PCIBIOS_DEVICE_NOT_FOUND;    				\
}

static int
null_read_config(struct pci_bus *bus, unsigned int devfn, int offset,
		 int len, u32 *val)
{
	return PCIBIOS_DEVICE_NOT_FOUND;
}

static int
null_write_config(struct pci_bus *bus, unsigned int devfn, int offset,
		  int len, u32 val)
{
	return PCIBIOS_DEVICE_NOT_FOUND;
}

static struct pci_ops null_pci_ops =
{
	.read = null_read_config,
	.write = null_write_config,
};

static struct pci_bus *
fake_pci_bus(struct pci_controller *hose, int busnr)
{
	static struct pci_bus bus;

	if (hose == 0) {
		hose = pci_bus_to_hose(busnr);
		if (hose == 0)
			printk(KERN_ERR "Can't find hose for PCI bus %d!\n", busnr);
	}
	bus.number = busnr;
	bus.sysdata = hose;
	bus.ops = hose? hose->ops: &null_pci_ops;
	return &bus;
}

#define EARLY_PCI_OP(rw, size, type)					\
int early_##rw##_config_##size(struct pci_controller *hose, int bus,	\
			       int devfn, int offset, type value)	\
{									\
	return pci_bus_##rw##_config_##size(fake_pci_bus(hose, bus),	\
					    devfn, offset, value);	\
}

EARLY_PCI_OP(read, byte, u8 *)
EARLY_PCI_OP(read, word, u16 *)
EARLY_PCI_OP(read, dword, u32 *)
EARLY_PCI_OP(write, byte, u8)
EARLY_PCI_OP(write, word, u16)
EARLY_PCI_OP(write, dword, u32)

extern int pci_bus_find_capability (struct pci_bus *bus, unsigned int devfn, int cap);
int early_find_capability(struct pci_controller *hose, int bus, int devfn,
			  int cap)
{
	return pci_bus_find_capability(fake_pci_bus(hose, bus), devfn, cap);
}
