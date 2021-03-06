

#include <linux/module.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/interrupt.h>
#include <linux/spinlock.h>
#include <linux/uaccess.h>
#include <linux/mm.h>
#include <linux/kfifo.h>

#include <linux/firmware.h>
#include <linux/syscalls.h>
#include <linux/uaccess.h>
#include <linux/platform_device.h>
#include <linux/proc_fs.h>

#include <mach/mt6516_pll.h>
#include <mach/mt6516_boot.h>

#define MOD "BOOT"

/* this vairable will be set by mt6516_fixup */
BOOTMODE g_boot_mode = UNKNOWN_BOOT;

static ssize_t (*md_show)(char*) = NULL;
static ssize_t (*md_store)(const char*,size_t) = NULL;

void boot_register_md_func(ssize_t (*show)(char*), ssize_t (*store)(const char*,size_t))
{
	md_show = show;
	md_store = store;
}

static ssize_t boot_show(struct kobject *kobj, struct attribute *a, char *buf)
{
	if (!strncmp(a->name, MD_SYSFS_ATTR, strlen(MD_SYSFS_ATTR)) && md_show) {
		return md_show(buf);
	}
	else
            return sprintf(buf, "%d\n", g_boot_mode);
}

static ssize_t boot_store(struct kobject *kobj, struct attribute *a, const char *buf, size_t count)
{
    if (!strncmp(a->name, MD_SYSFS_ATTR, strlen(MD_SYSFS_ATTR)) && md_store) {
	    return md_store(buf, count);
    }
    return count;
}


/* boot object */
static struct kobject boot_kobj;
static struct sysfs_ops boot_sysfs_ops = {
    .show = boot_show,
    .store = boot_store
};

/* boot attribute */
struct attribute boot_attr = {BOOT_SYSFS_ATTR, THIS_MODULE, 0644};
struct attribute md_attr = {MD_SYSFS_ATTR, THIS_MODULE, 0644};
static struct attribute *boot_attrs[] = {
    &boot_attr,
    &md_attr,
    NULL
};

/* boot type */
static struct kobj_type boot_ktype = {
    .sysfs_ops = &boot_sysfs_ops,
    .default_attrs = boot_attrs
};

/* boot device node */
static dev_t boot_dev_num;
static struct cdev boot_cdev;
static struct file_operations boot_fops = {
    .owner = THIS_MODULE,
    .open = NULL,
    .release = NULL,
    .write = NULL,
    .read = NULL,
    .ioctl = NULL
};

/* boot device class */
static struct class *boot_class;
static struct device *boot_device;


/* return boot mode */
BOOTMODE get_boot_mode(void)
{	return g_boot_mode;
}

/* for convenience, simply check is meta mode or not */
bool is_meta_mode(void)
{	if(g_boot_mode == META_BOOT)
	{	return true;
	}
	else
	{	return false;
	}
}

static int boot_mode_proc(char *page, char **start, off_t off,int count, int *eof, void *data)
{
  char *p = page;
  int len = 0; 

  p += sprintf(p, "\n\rMT6516 BOOT MODE : " );
  switch(g_boot_mode)
  {
  	case NORMAL_BOOT :
		  p += sprintf(p, "NORMAL BOOT\n");  		
		  break;
  	case META_BOOT :
		  p += sprintf(p, "META BOOT\n");  		
		  break;
	default :	  	
		  p += sprintf(p, "UNKNOWN BOOT\n");  		
		  break;
  }  
  *start = page + off;
  len = p - page;
  if (len > off)
		len -= off;
  else
		len = 0;

  return len < count ? len  : count;     
}

static int __init boot_mod_init(void)
{
    int ret;

    /* allocate device major number */
    if (alloc_chrdev_region(&boot_dev_num, 0, 1, BOOT_DEV_NAME) < 0) {
        printk("[%s] fail to register chrdev\n",MOD);
        return -1;
    }

    /* add character driver */
    cdev_init(&boot_cdev, &boot_fops);
    ret = cdev_add(&boot_cdev, boot_dev_num, 1);
    if (ret < 0) {
        printk("[%s] fail to add cdev\n",MOD);
        return ret;
    }

	/* create class (device model) */
    boot_class = class_create(THIS_MODULE, BOOT_DEV_NAME);
    if (IS_ERR(boot_class)) {
        printk("[%s] fail to create class\n",MOD);
        return (int)boot_class;
    }

    boot_device = device_create(boot_class, NULL, boot_dev_num, NULL, BOOT_DEV_NAME);
    if (IS_ERR(boot_device)) {
        printk("[%s] fail to create device\n",MOD);
        return (int)boot_device;
    }

    /* add kobject */
    ret = kobject_init_and_add(&boot_kobj, &boot_ktype, &(boot_device->kobj), BOOT_SYSFS);
    if (ret < 0) {
        printk("[%s] fail to add kobject\n",MOD);
        return ret;
    }

    /* create proc entry at /proc/boot_mode */
    create_proc_read_entry("boot_mode", S_IRUGO, NULL, boot_mode_proc, NULL);

    return 0;
}

static void __exit boot_mod_exit(void)
{
    cdev_del(&boot_cdev);
}

module_init(boot_mod_init);
module_exit(boot_mod_exit);
MODULE_DESCRIPTION("MT6516 Boot Information Querying Driver");
MODULE_LICENSE("Proprietary");
EXPORT_SYMBOL(is_meta_mode);
EXPORT_SYMBOL(get_boot_mode);
EXPORT_SYMBOL(boot_register_md_func);

