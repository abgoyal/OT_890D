

#include <linux/module.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/string.h>
#include <linux/errno.h>
#include <linux/init.h>
#include <linux/kmod.h>
#include <linux/slab.h>
#include <linux/smp_lock.h>
#include <asm/uaccess.h>
#include <asm/system.h>

#include <media/v4l2-common.h>
#include <media/v4l2-device.h>
#include <media/v4l2-ioctl.h>

#define VIDEO_NUM_DEVICES	256
#define VIDEO_NAME              "video4linux"


static ssize_t show_index(struct device *cd,
			 struct device_attribute *attr, char *buf)
{
	struct video_device *vdev = to_video_device(cd);

	return sprintf(buf, "%i\n", vdev->index);
}

static ssize_t show_name(struct device *cd,
			 struct device_attribute *attr, char *buf)
{
	struct video_device *vdev = to_video_device(cd);

	return sprintf(buf, "%.*s\n", (int)sizeof(vdev->name), vdev->name);
}

static struct device_attribute video_device_attrs[] = {
	__ATTR(name, S_IRUGO, show_name, NULL),
	__ATTR(index, S_IRUGO, show_index, NULL),
	__ATTR_NULL
};

static struct video_device *video_device[VIDEO_NUM_DEVICES];
static DEFINE_MUTEX(videodev_lock);
static DECLARE_BITMAP(video_nums[VFL_TYPE_MAX], VIDEO_NUM_DEVICES);

struct video_device *video_device_alloc(void)
{
	return kzalloc(sizeof(struct video_device), GFP_KERNEL);
}
EXPORT_SYMBOL(video_device_alloc);

void video_device_release(struct video_device *vdev)
{
	kfree(vdev);
}
EXPORT_SYMBOL(video_device_release);

void video_device_release_empty(struct video_device *vdev)
{
	/* Do nothing */
	/* Only valid when the video_device struct is a static. */
}
EXPORT_SYMBOL(video_device_release_empty);

static inline void video_get(struct video_device *vdev)
{
	get_device(&vdev->dev);
}

static inline void video_put(struct video_device *vdev)
{
	put_device(&vdev->dev);
}

/* Called when the last user of the video device exits. */
static void v4l2_device_release(struct device *cd)
{
	struct video_device *vdev = to_video_device(cd);

	mutex_lock(&videodev_lock);
	if (video_device[vdev->minor] != vdev) {
		mutex_unlock(&videodev_lock);
		/* should not happen */
		WARN_ON(1);
		return;
	}

	/* Free up this device for reuse */
	video_device[vdev->minor] = NULL;

	/* Delete the cdev on this minor as well */
	cdev_del(vdev->cdev);
	/* Just in case some driver tries to access this from
	   the release() callback. */
	vdev->cdev = NULL;

	/* Mark minor as free */
	clear_bit(vdev->num, video_nums[vdev->vfl_type]);

	mutex_unlock(&videodev_lock);

	/* Release video_device and perform other
	   cleanups as needed. */
	vdev->release(vdev);
}

static struct class video_class = {
	.name = VIDEO_NAME,
	.dev_attrs = video_device_attrs,
};

struct video_device *video_devdata(struct file *file)
{
	return video_device[iminor(file->f_path.dentry->d_inode)];
}
EXPORT_SYMBOL(video_devdata);

static ssize_t v4l2_read(struct file *filp, char __user *buf,
		size_t sz, loff_t *off)
{
	struct video_device *vdev = video_devdata(filp);

	if (!vdev->fops->read)
		return -EINVAL;
	if (video_is_unregistered(vdev))
		return -EIO;
	return vdev->fops->read(filp, buf, sz, off);
}

static ssize_t v4l2_write(struct file *filp, const char __user *buf,
		size_t sz, loff_t *off)
{
	struct video_device *vdev = video_devdata(filp);

	if (!vdev->fops->write)
		return -EINVAL;
	if (video_is_unregistered(vdev))
		return -EIO;
	return vdev->fops->write(filp, buf, sz, off);
}

static unsigned int v4l2_poll(struct file *filp, struct poll_table_struct *poll)
{
	struct video_device *vdev = video_devdata(filp);

	if (!vdev->fops->poll || video_is_unregistered(vdev))
		return DEFAULT_POLLMASK;
	return vdev->fops->poll(filp, poll);
}

static int v4l2_ioctl(struct inode *inode, struct file *filp,
		unsigned int cmd, unsigned long arg)
{
	struct video_device *vdev = video_devdata(filp);

	if (!vdev->fops->ioctl)
		return -ENOTTY;
	/* Allow ioctl to continue even if the device was unregistered.
	   Things like dequeueing buffers might still be useful. */
	return vdev->fops->ioctl(filp, cmd, arg);
}

static long v4l2_unlocked_ioctl(struct file *filp,
		unsigned int cmd, unsigned long arg)
{
	struct video_device *vdev = video_devdata(filp);

	if (!vdev->fops->unlocked_ioctl)
		return -ENOTTY;
	/* Allow ioctl to continue even if the device was unregistered.
	   Things like dequeueing buffers might still be useful. */
	return vdev->fops->unlocked_ioctl(filp, cmd, arg);
}

static int v4l2_mmap(struct file *filp, struct vm_area_struct *vm)
{
	struct video_device *vdev = video_devdata(filp);

	if (!vdev->fops->mmap ||
	    video_is_unregistered(vdev))
		return -ENODEV;
	return vdev->fops->mmap(filp, vm);
}

/* Override for the open function */
static int v4l2_open(struct inode *inode, struct file *filp)
{
	struct video_device *vdev;
	int ret;

	/* Check if the video device is available */
	mutex_lock(&videodev_lock);
	vdev = video_devdata(filp);
	/* return ENODEV if the video device has been removed
	   already or if it is not registered anymore. */
	if (vdev == NULL || video_is_unregistered(vdev)) {
		mutex_unlock(&videodev_lock);
		return -ENODEV;
	}
	/* and increase the device refcount */
	video_get(vdev);
	mutex_unlock(&videodev_lock);
	ret = vdev->fops->open(filp);
	/* decrease the refcount in case of an error */
	if (ret)
		video_put(vdev);
	return ret;
}

/* Override for the release function */
static int v4l2_release(struct inode *inode, struct file *filp)
{
	struct video_device *vdev = video_devdata(filp);
	int ret = vdev->fops->release(filp);

	/* decrease the refcount unconditionally since the release()
	   return value is ignored. */
	video_put(vdev);
	return ret;
}

static const struct file_operations v4l2_unlocked_fops = {
	.owner = THIS_MODULE,
	.read = v4l2_read,
	.write = v4l2_write,
	.open = v4l2_open,
	.mmap = v4l2_mmap,
	.unlocked_ioctl = v4l2_unlocked_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = v4l2_compat_ioctl32,
#endif
	.release = v4l2_release,
	.poll = v4l2_poll,
	.llseek = no_llseek,
};

static const struct file_operations v4l2_fops = {
	.owner = THIS_MODULE,
	.read = v4l2_read,
	.write = v4l2_write,
	.open = v4l2_open,
	.mmap = v4l2_mmap,
	.ioctl = v4l2_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = v4l2_compat_ioctl32,
#endif
	.release = v4l2_release,
	.poll = v4l2_poll,
	.llseek = no_llseek,
};

static int get_index(struct video_device *vdev, int num)
{
	u32 used = 0;
	const int max_index = sizeof(used) * 8 - 1;
	int i;

	/* Currently a single v4l driver instance cannot create more than
	   32 devices.
	   Increase to u64 or an array of u32 if more are needed. */
	if (num > max_index) {
		printk(KERN_ERR "videodev: %s num is too large\n", __func__);
		return -EINVAL;
	}

	/* Some drivers do not set the parent. In that case always return 0. */
	if (vdev->parent == NULL)
		return 0;

	for (i = 0; i < VIDEO_NUM_DEVICES; i++) {
		if (video_device[i] != NULL &&
		    video_device[i]->parent == vdev->parent) {
			used |= 1 << video_device[i]->index;
		}
	}

	if (num >= 0) {
		if (used & (1 << num))
			return -ENFILE;
		return num;
	}

	i = ffz(used);
	return i > max_index ? -ENFILE : i;
}

int video_register_device(struct video_device *vdev, int type, int nr)
{
	return video_register_device_index(vdev, type, nr, -1);
}
EXPORT_SYMBOL(video_register_device);

int video_register_device_index(struct video_device *vdev, int type, int nr,
					int index)
{
	int i = 0;
	int ret;
	int minor_offset = 0;
	int minor_cnt = VIDEO_NUM_DEVICES;
	const char *name_base;
	void *priv = video_get_drvdata(vdev);

	/* A minor value of -1 marks this video device as never
	   having been registered */
	if (vdev)
		vdev->minor = -1;

	/* the release callback MUST be present */
	WARN_ON(!vdev || !vdev->release);
	if (!vdev || !vdev->release)
		return -EINVAL;

	/* Part 1: check device type */
	switch (type) {
	case VFL_TYPE_GRABBER:
		name_base = "video";
		break;
	case VFL_TYPE_VTX:
		name_base = "vtx";
		break;
	case VFL_TYPE_VBI:
		name_base = "vbi";
		break;
	case VFL_TYPE_RADIO:
		name_base = "radio";
		break;
	default:
		printk(KERN_ERR "%s called with unknown type: %d\n",
		       __func__, type);
		return -EINVAL;
	}

	vdev->vfl_type = type;
	vdev->cdev = NULL;
	if (vdev->v4l2_dev)
		vdev->parent = vdev->v4l2_dev->dev;

	/* Part 2: find a free minor, kernel number and device index. */
#ifdef CONFIG_VIDEO_FIXED_MINOR_RANGES
	/* Keep the ranges for the first four types for historical
	 * reasons.
	 * Newer devices (not yet in place) should use the range
	 * of 128-191 and just pick the first free minor there
	 * (new style). */
	switch (type) {
	case VFL_TYPE_GRABBER:
		minor_offset = 0;
		minor_cnt = 64;
		break;
	case VFL_TYPE_RADIO:
		minor_offset = 64;
		minor_cnt = 64;
		break;
	case VFL_TYPE_VTX:
		minor_offset = 192;
		minor_cnt = 32;
		break;
	case VFL_TYPE_VBI:
		minor_offset = 224;
		minor_cnt = 32;
		break;
	default:
		minor_offset = 128;
		minor_cnt = 64;
		break;
	}
#endif

	/* Pick a minor number */
	mutex_lock(&videodev_lock);
	nr = find_next_zero_bit(video_nums[type], minor_cnt, nr == -1 ? 0 : nr);
	if (nr == minor_cnt)
		nr = find_first_zero_bit(video_nums[type], minor_cnt);
	if (nr == minor_cnt) {
		printk(KERN_ERR "could not get a free kernel number\n");
		mutex_unlock(&videodev_lock);
		return -ENFILE;
	}
#ifdef CONFIG_VIDEO_FIXED_MINOR_RANGES
	/* 1-on-1 mapping of kernel number to minor number */
	i = nr;
#else
	/* The kernel number and minor numbers are independent */
	for (i = 0; i < VIDEO_NUM_DEVICES; i++)
		if (video_device[i] == NULL)
			break;
	if (i == VIDEO_NUM_DEVICES) {
		mutex_unlock(&videodev_lock);
		printk(KERN_ERR "could not get a free minor\n");
		return -ENFILE;
	}
#endif
	vdev->minor = i + minor_offset;
	vdev->num = nr;
	set_bit(nr, video_nums[type]);
	/* Should not happen since we thought this minor was free */
	WARN_ON(video_device[vdev->minor] != NULL);
	ret = vdev->index = get_index(vdev, index);
	mutex_unlock(&videodev_lock);

	if (ret < 0) {
		printk(KERN_ERR "%s: get_index failed\n", __func__);
		goto cleanup;
	}

	/* Part 3: Initialize the character device */
	vdev->cdev = cdev_alloc();
	if (vdev->cdev == NULL) {
		ret = -ENOMEM;
		goto cleanup;
	}
	if (vdev->fops->unlocked_ioctl)
		vdev->cdev->ops = &v4l2_unlocked_fops;
	else
		vdev->cdev->ops = &v4l2_fops;
	vdev->cdev->owner = vdev->fops->owner;
	ret = cdev_add(vdev->cdev, MKDEV(VIDEO_MAJOR, vdev->minor), 1);
	if (ret < 0) {
		printk(KERN_ERR "%s: cdev_add failed\n", __func__);
		kfree(vdev->cdev);
		vdev->cdev = NULL;
		goto cleanup;
	}

	/* Part 4: register the device with sysfs */
	memset(&vdev->dev, 0, sizeof(vdev->dev));
	/* The memset above cleared the device's drvdata, so
	   put back the copy we made earlier. */
	video_set_drvdata(vdev, priv);
	vdev->dev.class = &video_class;
	vdev->dev.devt = MKDEV(VIDEO_MAJOR, vdev->minor);
	if (vdev->parent)
		vdev->dev.parent = vdev->parent;
	dev_set_name(&vdev->dev, "%s%d", name_base, nr);
	ret = device_register(&vdev->dev);
	if (ret < 0) {
		printk(KERN_ERR "%s: device_register failed\n", __func__);
		goto cleanup;
	}
	/* Register the release callback that will be called when the last
	   reference to the device goes away. */
	vdev->dev.release = v4l2_device_release;

	/* Part 5: Activate this minor. The char device can now be used. */
	mutex_lock(&videodev_lock);
	video_device[vdev->minor] = vdev;
	mutex_unlock(&videodev_lock);
	return 0;

cleanup:
	mutex_lock(&videodev_lock);
	if (vdev->cdev)
		cdev_del(vdev->cdev);
	clear_bit(vdev->num, video_nums[type]);
	mutex_unlock(&videodev_lock);
	/* Mark this video device as never having been registered. */
	vdev->minor = -1;
	return ret;
}
EXPORT_SYMBOL(video_register_device_index);

void video_unregister_device(struct video_device *vdev)
{
	/* Check if vdev was ever registered at all */
	if (!vdev || vdev->minor < 0)
		return;

	mutex_lock(&videodev_lock);
	set_bit(V4L2_FL_UNREGISTERED, &vdev->flags);
	mutex_unlock(&videodev_lock);
	device_unregister(&vdev->dev);
}
EXPORT_SYMBOL(video_unregister_device);

static int __init videodev_init(void)
{
	dev_t dev = MKDEV(VIDEO_MAJOR, 0);
	int ret;

	printk(KERN_INFO "Linux video capture interface: v2.00\n");
	ret = register_chrdev_region(dev, VIDEO_NUM_DEVICES, VIDEO_NAME);
	if (ret < 0) {
		printk(KERN_WARNING "videodev: unable to get major %d\n",
				VIDEO_MAJOR);
		return ret;
	}

	ret = class_register(&video_class);
	if (ret < 0) {
		unregister_chrdev_region(dev, VIDEO_NUM_DEVICES);
		printk(KERN_WARNING "video_dev: class_register failed\n");
		return -EIO;
	}

	return 0;
}

static void __exit videodev_exit(void)
{
	dev_t dev = MKDEV(VIDEO_MAJOR, 0);

	class_unregister(&video_class);
	unregister_chrdev_region(dev, VIDEO_NUM_DEVICES);
}

module_init(videodev_init)
module_exit(videodev_exit)

MODULE_AUTHOR("Alan Cox, Mauro Carvalho Chehab <mchehab@infradead.org>");
MODULE_DESCRIPTION("Device registrar for Video4Linux drivers v2");
MODULE_LICENSE("GPL");


