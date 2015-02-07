




/* #define VERBOSE_DEBUG */

#include <linux/kernel.h>
#include <linux/utsname.h>
#include <linux/device.h>

#include "g_zero.h"
#include "gadget_chips.h"


/*-------------------------------------------------------------------------*/

#include "composite.c"
#include "usbstring.c"
#include "config.c"
#include "epautoconf.c"

#include "f_sourcesink.c"
#include "f_loopback.c"

/*-------------------------------------------------------------------------*/

#define DRIVER_VERSION		"Cinco de Mayo 2008"

static const char longname[] = "Gadget Zero";

unsigned buflen = 4096;
module_param(buflen, uint, 0);

static int loopdefault = 0;
module_param(loopdefault, bool, S_IRUGO|S_IWUSR);

/*-------------------------------------------------------------------------*/

#ifndef	CONFIG_USB_ZERO_HNPTEST
#define DRIVER_VENDOR_NUM	0x0525		/* NetChip */
#define DRIVER_PRODUCT_NUM	0xa4a0		/* Linux-USB "Gadget Zero" */
#else
#define DRIVER_VENDOR_NUM	0x1a0a		/* OTG test device IDs */
#define DRIVER_PRODUCT_NUM	0xbadd
#endif

/*-------------------------------------------------------------------------*/

static struct usb_device_descriptor device_desc = {
	.bLength =		sizeof device_desc,
	.bDescriptorType =	USB_DT_DEVICE,

	.bcdUSB =		__constant_cpu_to_le16(0x0200),
	.bDeviceClass =		USB_CLASS_VENDOR_SPEC,

	.idVendor =		__constant_cpu_to_le16(DRIVER_VENDOR_NUM),
	.idProduct =		__constant_cpu_to_le16(DRIVER_PRODUCT_NUM),
	.bNumConfigurations =	2,
};

#ifdef CONFIG_USB_OTG
static struct usb_otg_descriptor otg_descriptor = {
	.bLength =		sizeof otg_descriptor,
	.bDescriptorType =	USB_DT_OTG,

	/* REVISIT SRP-only hardware is possible, although
	 * it would not be called "OTG" ...
	 */
	.bmAttributes =		USB_OTG_SRP | USB_OTG_HNP,
};

const struct usb_descriptor_header *otg_desc[] = {
	(struct usb_descriptor_header *) &otg_descriptor,
	NULL,
};
#endif

/* string IDs are assigned dynamically */

#define STRING_MANUFACTURER_IDX		0
#define STRING_PRODUCT_IDX		1
#define STRING_SERIAL_IDX		2

static char manufacturer[50];

/* default serial number takes at least two packets */
static char serial[] = "0123456789.0123456789.0123456789";

static struct usb_string strings_dev[] = {
	[STRING_MANUFACTURER_IDX].s = manufacturer,
	[STRING_PRODUCT_IDX].s = longname,
	[STRING_SERIAL_IDX].s = serial,
	{  }			/* end of list */
};

static struct usb_gadget_strings stringtab_dev = {
	.language	= 0x0409,	/* en-us */
	.strings	= strings_dev,
};

static struct usb_gadget_strings *dev_strings[] = {
	&stringtab_dev,
	NULL,
};

/*-------------------------------------------------------------------------*/

struct usb_request *alloc_ep_req(struct usb_ep *ep)
{
	struct usb_request	*req;

	req = usb_ep_alloc_request(ep, GFP_ATOMIC);
	if (req) {
		req->length = buflen;
		req->buf = kmalloc(buflen, GFP_ATOMIC);
		if (!req->buf) {
			usb_ep_free_request(ep, req);
			req = NULL;
		}
	}
	return req;
}

void free_ep_req(struct usb_ep *ep, struct usb_request *req)
{
	kfree(req->buf);
	usb_ep_free_request(ep, req);
}

static void disable_ep(struct usb_composite_dev *cdev, struct usb_ep *ep)
{
	int			value;

	if (ep->driver_data) {
		value = usb_ep_disable(ep);
		if (value < 0)
			DBG(cdev, "disable %s --> %d\n",
					ep->name, value);
		ep->driver_data = NULL;
	}
}

void disable_endpoints(struct usb_composite_dev *cdev,
		struct usb_ep *in, struct usb_ep *out)
{
	disable_ep(cdev, in);
	disable_ep(cdev, out);
}

/*-------------------------------------------------------------------------*/

static int __init zero_bind(struct usb_composite_dev *cdev)
{
	int			gcnum;
	struct usb_gadget	*gadget = cdev->gadget;
	int			id;

	/* Allocate string descriptor numbers ... note that string
	 * contents can be overridden by the composite_dev glue.
	 */
	id = usb_string_id(cdev);
	if (id < 0)
		return id;
	strings_dev[STRING_MANUFACTURER_IDX].id = id;
	device_desc.iManufacturer = id;

	id = usb_string_id(cdev);
	if (id < 0)
		return id;
	strings_dev[STRING_PRODUCT_IDX].id = id;
	device_desc.iProduct = id;

	id = usb_string_id(cdev);
	if (id < 0)
		return id;
	strings_dev[STRING_SERIAL_IDX].id = id;
	device_desc.iSerialNumber = id;

	/* Register primary, then secondary configuration.  Note that
	 * SH3 only allows one config...
	 */
	if (loopdefault) {
		loopback_add(cdev);
		if (!gadget_is_sh(gadget))
			sourcesink_add(cdev);
	} else {
		sourcesink_add(cdev);
		if (!gadget_is_sh(gadget))
			loopback_add(cdev);
	}

	gcnum = usb_gadget_controller_number(gadget);
	if (gcnum >= 0)
		device_desc.bcdDevice = cpu_to_le16(0x0200 + gcnum);
	else {
		/* gadget zero is so simple (for now, no altsettings) that
		 * it SHOULD NOT have problems with bulk-capable hardware.
		 * so just warn about unrcognized controllers -- don't panic.
		 *
		 * things like configuration and altsetting numbering
		 * can need hardware-specific attention though.
		 */
		pr_warning("%s: controller '%s' not recognized\n",
			longname, gadget->name);
		device_desc.bcdDevice = __constant_cpu_to_le16(0x9999);
	}


	INFO(cdev, "%s, version: " DRIVER_VERSION "\n", longname);

	snprintf(manufacturer, sizeof manufacturer, "%s %s with %s",
		init_utsname()->sysname, init_utsname()->release,
		gadget->name);

	return 0;
}

static struct usb_composite_driver zero_driver = {
	.name		= "zero",
	.dev		= &device_desc,
	.strings	= dev_strings,
	.bind		= zero_bind,
};

MODULE_AUTHOR("David Brownell");
MODULE_LICENSE("GPL");

static int __init init(void)
{
	return usb_composite_register(&zero_driver);
}
module_init(init);

static void __exit cleanup(void)
{
	usb_composite_unregister(&zero_driver);
}
module_exit(cleanup);