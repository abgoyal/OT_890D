

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/tty.h>
#include <linux/module.h>
#include <linux/usb.h>
#include <linux/usb/serial.h>

static struct usb_device_id id_table [] = {
	{ USB_DEVICE(0x05c6, 0x3197) },	/* unknown Motorola phone */
	{ USB_DEVICE(0x0c44, 0x0022) },	/* unknown Mororola phone */
	{ USB_DEVICE(0x22b8, 0x2a64) },	/* Motorola KRZR K1m */
	{ },
};
MODULE_DEVICE_TABLE(usb, id_table);

static struct usb_driver moto_driver = {
	.name =		"moto-modem",
	.probe =	usb_serial_probe,
	.disconnect =	usb_serial_disconnect,
	.id_table =	id_table,
	.no_dynamic_id = 	1,
};

static struct usb_serial_driver moto_device = {
	.driver = {
		.owner =	THIS_MODULE,
		.name =		"moto-modem",
	},
	.id_table =		id_table,
	.num_ports =		1,
};

static int __init moto_init(void)
{
	int retval;

	retval = usb_serial_register(&moto_device);
	if (retval)
		return retval;
	retval = usb_register(&moto_driver);
	if (retval)
		usb_serial_deregister(&moto_device);
	return retval;
}

static void __exit moto_exit(void)
{
	usb_deregister(&moto_driver);
	usb_serial_deregister(&moto_device);
}

module_init(moto_init);
module_exit(moto_exit);
MODULE_LICENSE("GPL");
