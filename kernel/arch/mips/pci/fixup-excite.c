
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/pci.h>
#include <excite.h>

int __init pcibios_map_irq(const struct pci_dev *dev, u8 slot, u8 pin)
{
	if (pin == 0)
		return -1;

	return USB_IRQ;		/* USB controller is the only PCI device */
}

/* Do platform specific device initialization at pci_enable_device() time */
int pcibios_plat_dev_init(struct pci_dev *dev)
{
	return 0;
}
