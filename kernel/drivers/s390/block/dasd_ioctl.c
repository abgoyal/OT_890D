
#include <linux/interrupt.h>
#include <linux/major.h>
#include <linux/fs.h>
#include <linux/blkpg.h>

#include <asm/ccwdev.h>
#include <asm/cmb.h>
#include <asm/uaccess.h>

/* This is ugly... */
#define PRINTK_HEADER "dasd_ioctl:"

#include "dasd_int.h"


static int
dasd_ioctl_api_version(void __user *argp)
{
	int ver = DASD_API_VERSION;
	return put_user(ver, (int __user *)argp);
}

static int
dasd_ioctl_enable(struct block_device *bdev)
{
	struct dasd_block *block = bdev->bd_disk->private_data;

	if (!capable(CAP_SYS_ADMIN))
		return -EACCES;

	dasd_enable_device(block->base);
	/* Formatting the dasd device can change the capacity. */
	mutex_lock(&bdev->bd_mutex);
	i_size_write(bdev->bd_inode, (loff_t)get_capacity(block->gdp) << 9);
	mutex_unlock(&bdev->bd_mutex);
	return 0;
}

static int
dasd_ioctl_disable(struct block_device *bdev)
{
	struct dasd_block *block = bdev->bd_disk->private_data;

	if (!capable(CAP_SYS_ADMIN))
		return -EACCES;

	/*
	 * Man this is sick. We don't do a real disable but only downgrade
	 * the device to DASD_STATE_BASIC. The reason is that dasdfmt uses
	 * BIODASDDISABLE to disable accesses to the device via the block
	 * device layer but it still wants to do i/o on the device by
	 * using the BIODASDFMT ioctl. Therefore the correct state for the
	 * device is DASD_STATE_BASIC that allows to do basic i/o.
	 */
	dasd_set_target_state(block->base, DASD_STATE_BASIC);
	/*
	 * Set i_size to zero, since read, write, etc. check against this
	 * value.
	 */
	mutex_lock(&bdev->bd_mutex);
	i_size_write(bdev->bd_inode, 0);
	mutex_unlock(&bdev->bd_mutex);
	return 0;
}

static int dasd_ioctl_quiesce(struct dasd_block *block)
{
	unsigned long flags;
	struct dasd_device *base;

	base = block->base;
	if (!capable (CAP_SYS_ADMIN))
		return -EACCES;

	DEV_MESSAGE(KERN_DEBUG, base, "%s", "Quiesce IO on device");
	spin_lock_irqsave(get_ccwdev_lock(base->cdev), flags);
	base->stopped |= DASD_STOPPED_QUIESCE;
	spin_unlock_irqrestore(get_ccwdev_lock(base->cdev), flags);
	return 0;
}


static int dasd_ioctl_resume(struct dasd_block *block)
{
	unsigned long flags;
	struct dasd_device *base;

	base = block->base;
	if (!capable (CAP_SYS_ADMIN))
		return -EACCES;

	DEV_MESSAGE(KERN_DEBUG, base, "%s", "resume IO on device");
	spin_lock_irqsave(get_ccwdev_lock(base->cdev), flags);
	base->stopped &= ~DASD_STOPPED_QUIESCE;
	spin_unlock_irqrestore(get_ccwdev_lock(base->cdev), flags);

	dasd_schedule_block_bh(block);
	return 0;
}

static int dasd_format(struct dasd_block *block, struct format_data_t *fdata)
{
	struct dasd_ccw_req *cqr;
	struct dasd_device *base;
	int rc;

	base = block->base;
	if (base->discipline->format_device == NULL)
		return -EPERM;

	if (base->state != DASD_STATE_BASIC) {
		DEV_MESSAGE(KERN_WARNING, base, "%s",
			    "dasd_format: device is not disabled! ");
		return -EBUSY;
	}

	DBF_DEV_EVENT(DBF_NOTICE, base,
		      "formatting units %d to %d (%d B blocks) flags %d",
		      fdata->start_unit,
		      fdata->stop_unit, fdata->blksize, fdata->intensity);

	/* Since dasdfmt keeps the device open after it was disabled,
	 * there still exists an inode for this device.
	 * We must update i_blkbits, otherwise we might get errors when
	 * enabling the device later.
	 */
	if (fdata->start_unit == 0) {
		struct block_device *bdev = bdget_disk(block->gdp, 0);
		bdev->bd_inode->i_blkbits = blksize_bits(fdata->blksize);
		bdput(bdev);
	}

	while (fdata->start_unit <= fdata->stop_unit) {
		cqr = base->discipline->format_device(base, fdata);
		if (IS_ERR(cqr))
			return PTR_ERR(cqr);
		rc = dasd_sleep_on_interruptible(cqr);
		dasd_sfree_request(cqr, cqr->memdev);
		if (rc) {
			if (rc != -ERESTARTSYS)
				DEV_MESSAGE(KERN_ERR, base,
					    " Formatting of unit %d failed "
					    "with rc = %d",
					    fdata->start_unit, rc);
			return rc;
		}
		fdata->start_unit++;
	}
	return 0;
}

static int
dasd_ioctl_format(struct block_device *bdev, void __user *argp)
{
	struct dasd_block *block = bdev->bd_disk->private_data;
	struct format_data_t fdata;

	if (!capable(CAP_SYS_ADMIN))
		return -EACCES;
	if (!argp)
		return -EINVAL;

	if (block->base->features & DASD_FEATURE_READONLY)
		return -EROFS;
	if (copy_from_user(&fdata, argp, sizeof(struct format_data_t)))
		return -EFAULT;
	if (bdev != bdev->bd_contains) {
		DEV_MESSAGE(KERN_WARNING, block->base, "%s",
			    "Cannot low-level format a partition");
		return -EINVAL;
	}
	return dasd_format(block, &fdata);
}

#ifdef CONFIG_DASD_PROFILE
static int dasd_ioctl_reset_profile(struct dasd_block *block)
{
	memset(&block->profile, 0, sizeof(struct dasd_profile_info_t));
	return 0;
}

static int dasd_ioctl_read_profile(struct dasd_block *block, void __user *argp)
{
	if (dasd_profile_level == DASD_PROFILE_OFF)
		return -EIO;
	if (copy_to_user(argp, &block->profile,
			 sizeof(struct dasd_profile_info_t)))
		return -EFAULT;
	return 0;
}
#else
static int dasd_ioctl_reset_profile(struct dasd_block *block)
{
	return -ENOSYS;
}

static int dasd_ioctl_read_profile(struct dasd_block *block, void __user *argp)
{
	return -ENOSYS;
}
#endif

static int dasd_ioctl_information(struct dasd_block *block,
				  unsigned int cmd, void __user *argp)
{
	struct dasd_information2_t *dasd_info;
	unsigned long flags;
	int rc;
	struct dasd_device *base;
	struct ccw_device *cdev;
	struct ccw_dev_id dev_id;

	base = block->base;
	if (!base->discipline->fill_info)
		return -EINVAL;

	dasd_info = kzalloc(sizeof(struct dasd_information2_t), GFP_KERNEL);
	if (dasd_info == NULL)
		return -ENOMEM;

	rc = base->discipline->fill_info(base, dasd_info);
	if (rc) {
		kfree(dasd_info);
		return rc;
	}

	cdev = base->cdev;
	ccw_device_get_id(cdev, &dev_id);

	dasd_info->devno = dev_id.devno;
	dasd_info->schid = _ccw_device_get_subchannel_number(base->cdev);
	dasd_info->cu_type = cdev->id.cu_type;
	dasd_info->cu_model = cdev->id.cu_model;
	dasd_info->dev_type = cdev->id.dev_type;
	dasd_info->dev_model = cdev->id.dev_model;
	dasd_info->status = base->state;
	/*
	 * The open_count is increased for every opener, that includes
	 * the blkdev_get in dasd_scan_partitions.
	 * This must be hidden from user-space.
	 */
	dasd_info->open_count = atomic_read(&block->open_count);
	if (!block->bdev)
		dasd_info->open_count++;

	/*
	 * check if device is really formatted
	 * LDL / CDL was returned by 'fill_info'
	 */
	if ((base->state < DASD_STATE_READY) ||
	    (dasd_check_blocksize(block->bp_block)))
		dasd_info->format = DASD_FORMAT_NONE;

	dasd_info->features |=
		((base->features & DASD_FEATURE_READONLY) != 0);

	if (base->discipline)
		memcpy(dasd_info->type, base->discipline->name, 4);
	else
		memcpy(dasd_info->type, "none", 4);

	if (block->request_queue->request_fn) {
		struct list_head *l;
#ifdef DASD_EXTENDED_PROFILING
		{
			struct list_head *l;
			spin_lock_irqsave(&block->lock, flags);
			list_for_each(l, &block->request_queue->queue_head)
				dasd_info->req_queue_len++;
			spin_unlock_irqrestore(&block->lock, flags);
		}
#endif				/* DASD_EXTENDED_PROFILING */
		spin_lock_irqsave(get_ccwdev_lock(base->cdev), flags);
		list_for_each(l, &base->ccw_queue)
			dasd_info->chanq_len++;
		spin_unlock_irqrestore(get_ccwdev_lock(base->cdev),
				       flags);
	}

	rc = 0;
	if (copy_to_user(argp, dasd_info,
			 ((cmd == (unsigned int) BIODASDINFO2) ?
			  sizeof(struct dasd_information2_t) :
			  sizeof(struct dasd_information_t))))
		rc = -EFAULT;
	kfree(dasd_info);
	return rc;
}

static int
dasd_ioctl_set_ro(struct block_device *bdev, void __user *argp)
{
	struct dasd_block *block =  bdev->bd_disk->private_data;
	int intval;

	if (!capable(CAP_SYS_ADMIN))
		return -EACCES;
	if (bdev != bdev->bd_contains)
		// ro setting is not allowed for partitions
		return -EINVAL;
	if (get_user(intval, (int __user *)argp))
		return -EFAULT;

	set_disk_ro(bdev->bd_disk, intval);
	return dasd_set_feature(block->base->cdev, DASD_FEATURE_READONLY, intval);
}

static int dasd_ioctl_readall_cmb(struct dasd_block *block, unsigned int cmd,
		unsigned long arg)
{
	struct cmbdata __user *argp = (void __user *) arg;
	size_t size = _IOC_SIZE(cmd);
	struct cmbdata data;
	int ret;

	ret = cmf_readall(block->base->cdev, &data);
	if (!ret && copy_to_user(argp, &data, min(size, sizeof(*argp))))
		return -EFAULT;
	return ret;
}

int
dasd_ioctl(struct block_device *bdev, fmode_t mode,
	   unsigned int cmd, unsigned long arg)
{
	struct dasd_block *block = bdev->bd_disk->private_data;
	void __user *argp = (void __user *)arg;

	if (!block)
                return -ENODEV;

	if ((_IOC_DIR(cmd) != _IOC_NONE) && !arg) {
		PRINT_DEBUG("empty data ptr");
		return -EINVAL;
	}

	switch (cmd) {
	case BIODASDDISABLE:
		return dasd_ioctl_disable(bdev);
	case BIODASDENABLE:
		return dasd_ioctl_enable(bdev);
	case BIODASDQUIESCE:
		return dasd_ioctl_quiesce(block);
	case BIODASDRESUME:
		return dasd_ioctl_resume(block);
	case BIODASDFMT:
		return dasd_ioctl_format(bdev, argp);
	case BIODASDINFO:
		return dasd_ioctl_information(block, cmd, argp);
	case BIODASDINFO2:
		return dasd_ioctl_information(block, cmd, argp);
	case BIODASDPRRD:
		return dasd_ioctl_read_profile(block, argp);
	case BIODASDPRRST:
		return dasd_ioctl_reset_profile(block);
	case BLKROSET:
		return dasd_ioctl_set_ro(bdev, argp);
	case DASDAPIVER:
		return dasd_ioctl_api_version(argp);
	case BIODASDCMFENABLE:
		return enable_cmf(block->base->cdev);
	case BIODASDCMFDISABLE:
		return disable_cmf(block->base->cdev);
	case BIODASDREADALLCMB:
		return dasd_ioctl_readall_cmb(block, cmd, arg);
	default:
		/* if the discipline has an ioctl method try it. */
		if (block->base->discipline->ioctl) {
			int rval = block->base->discipline->ioctl(block, cmd, argp);
			if (rval != -ENOIOCTLCMD)
				return rval;
		}

		return -EINVAL;
	}
}
