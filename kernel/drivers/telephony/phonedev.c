

#include <linux/module.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/mm.h>
#include <linux/string.h>
#include <linux/errno.h>
#include <linux/phonedev.h>
#include <linux/init.h>
#include <linux/smp_lock.h>
#include <asm/uaccess.h>
#include <asm/system.h>

#include <linux/kmod.h>
#include <linux/sem.h>
#include <linux/mutex.h>

#define PHONE_NUM_DEVICES	256


static struct phone_device *phone_device[PHONE_NUM_DEVICES];
static DEFINE_MUTEX(phone_lock);


static int phone_open(struct inode *inode, struct file *file)
{
	unsigned int minor = iminor(inode);
	int err = 0;
	struct phone_device *p;
	const struct file_operations *old_fops, *new_fops = NULL;

	if (minor >= PHONE_NUM_DEVICES)
		return -ENODEV;

	mutex_lock(&phone_lock);
	p = phone_device[minor];
	if (p)
		new_fops = fops_get(p->f_op);
	if (!new_fops) {
		mutex_unlock(&phone_lock);
		request_module("char-major-%d-%d", PHONE_MAJOR, minor);
		mutex_lock(&phone_lock);
		p = phone_device[minor];
		if (p == NULL || (new_fops = fops_get(p->f_op)) == NULL)
		{
			err=-ENODEV;
			goto end;
		}
	}
	old_fops = file->f_op;
	file->f_op = new_fops;
	if (p->open)
		err = p->open(p, file);	/* Tell the device it is open */
	if (err) {
		fops_put(file->f_op);
		file->f_op = fops_get(old_fops);
	}
	fops_put(old_fops);
end:
	mutex_unlock(&phone_lock);
	return err;
}


int phone_register_device(struct phone_device *p, int unit)
{
	int base;
	int end;
	int i;

	base = 0;
	end = PHONE_NUM_DEVICES - 1;

	if (unit != PHONE_UNIT_ANY) {
		base = unit;
		end = unit + 1;  /* enter the loop at least one time */
	}
	
	mutex_lock(&phone_lock);
	for (i = base; i < end; i++) {
		if (phone_device[i] == NULL) {
			phone_device[i] = p;
			p->minor = i;
			mutex_unlock(&phone_lock);
			return 0;
		}
	}
	mutex_unlock(&phone_lock);
	return -ENFILE;
}


void phone_unregister_device(struct phone_device *pfd)
{
	mutex_lock(&phone_lock);
	if (likely(phone_device[pfd->minor] == pfd))
		phone_device[pfd->minor] = NULL;
	mutex_unlock(&phone_lock);
}


static const struct file_operations phone_fops =
{
	.owner		= THIS_MODULE,
	.open		= phone_open,
};

 


static int __init telephony_init(void)
{
	printk(KERN_INFO "Linux telephony interface: v1.00\n");
	if (register_chrdev(PHONE_MAJOR, "telephony", &phone_fops)) {
		printk("phonedev: unable to get major %d\n", PHONE_MAJOR);
		return -EIO;
	}

	return 0;
}

static void __exit telephony_exit(void)
{
	unregister_chrdev(PHONE_MAJOR, "telephony");
}

module_init(telephony_init);
module_exit(telephony_exit);

MODULE_LICENSE("GPL");

EXPORT_SYMBOL(phone_register_device);
EXPORT_SYMBOL(phone_unregister_device);
