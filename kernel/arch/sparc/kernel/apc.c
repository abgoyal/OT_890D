

#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/errno.h>
#include <linux/init.h>
#include <linux/miscdevice.h>
#include <linux/smp_lock.h>
#include <linux/pm.h>
#include <linux/of.h>
#include <linux/of_device.h>

#include <asm/io.h>
#include <asm/oplib.h>
#include <asm/uaccess.h>
#include <asm/auxio.h>
#include <asm/apc.h>


#define APC_MINOR	MISC_DYNAMIC_MINOR
#define APC_OBPNAME	"power-management"
#define APC_DEVNAME "apc"

static u8 __iomem *regs;
static int apc_no_idle __devinitdata = 0;

#define apc_readb(offs)		(sbus_readb(regs+offs))
#define apc_writeb(val, offs) 	(sbus_writeb(val, regs+offs))

static int __init apc_setup(char *str) 
{
	if(!strncmp(str, "noidle", strlen("noidle"))) {
		apc_no_idle = 1;
		return 1;
	}
	return 0;
}
__setup("apc=", apc_setup);

static void apc_swift_idle(void)
{
#ifdef APC_DEBUG_LED
	set_auxio(0x00, AUXIO_LED); 
#endif

	apc_writeb(apc_readb(APC_IDLE_REG) | APC_IDLE_ON, APC_IDLE_REG);

#ifdef APC_DEBUG_LED
	set_auxio(AUXIO_LED, 0x00); 
#endif
} 

static inline void apc_free(struct of_device *op)
{
	of_iounmap(&op->resource[0], regs, resource_size(&op->resource[0]));
}

static int apc_open(struct inode *inode, struct file *f)
{
	cycle_kernel_lock();
	return 0;
}

static int apc_release(struct inode *inode, struct file *f)
{
	return 0;
}

static long apc_ioctl(struct file *f, unsigned int cmd, unsigned long __arg)
{
	__u8 inarg, __user *arg;

	arg = (__u8 __user *) __arg;

	lock_kernel();

	switch (cmd) {
	case APCIOCGFANCTL:
		if (put_user(apc_readb(APC_FANCTL_REG) & APC_REGMASK, arg)) {
			unlock_kernel();
			return -EFAULT;
		}
		break;

	case APCIOCGCPWR:
		if (put_user(apc_readb(APC_CPOWER_REG) & APC_REGMASK, arg)) {
			unlock_kernel();
			return -EFAULT;
		}
		break;

	case APCIOCGBPORT:
		if (put_user(apc_readb(APC_BPORT_REG) & APC_BPMASK, arg)) {
			unlock_kernel();
			return -EFAULT;
		}
		break;

	case APCIOCSFANCTL:
		if (get_user(inarg, arg)) {
			unlock_kernel();
			return -EFAULT;
		}
		apc_writeb(inarg & APC_REGMASK, APC_FANCTL_REG);
		break;
	case APCIOCSCPWR:
		if (get_user(inarg, arg)) {
			unlock_kernel();
			return -EFAULT;
		}
		apc_writeb(inarg & APC_REGMASK, APC_CPOWER_REG);
		break;
	case APCIOCSBPORT:
		if (get_user(inarg, arg)) {
			unlock_kernel();
			return -EFAULT;
		}
		apc_writeb(inarg & APC_BPMASK, APC_BPORT_REG);
		break;
	default:
		unlock_kernel();
		return -EINVAL;
	};

	unlock_kernel();
	return 0;
}

static const struct file_operations apc_fops = {
	.unlocked_ioctl =	apc_ioctl,
	.open =			apc_open,
	.release =		apc_release,
};

static struct miscdevice apc_miscdev = { APC_MINOR, APC_DEVNAME, &apc_fops };

static int __devinit apc_probe(struct of_device *op,
			       const struct of_device_id *match)
{
	int err;

	regs = of_ioremap(&op->resource[0], 0,
			  resource_size(&op->resource[0]), APC_OBPNAME);
	if (!regs) {
		printk(KERN_ERR "%s: unable to map registers\n", APC_DEVNAME);
		return -ENODEV;
	}

	err = misc_register(&apc_miscdev);
	if (err) {
		printk(KERN_ERR "%s: unable to register device\n", APC_DEVNAME);
		apc_free(op);
		return -ENODEV;
	}

	/* Assign power management IDLE handler */
	if (!apc_no_idle)
		pm_idle = apc_swift_idle;	

	printk(KERN_INFO "%s: power management initialized%s\n", 
	       APC_DEVNAME, apc_no_idle ? " (CPU idle disabled)" : "");

	return 0;
}

static struct of_device_id __initdata apc_match[] = {
	{
		.name = APC_OBPNAME,
	},
	{},
};
MODULE_DEVICE_TABLE(of, apc_match);

static struct of_platform_driver apc_driver = {
	.name		= "apc",
	.match_table	= apc_match,
	.probe		= apc_probe,
};

static int __init apc_init(void)
{
	return of_register_driver(&apc_driver, &of_bus_type);
}

__initcall(apc_init);
