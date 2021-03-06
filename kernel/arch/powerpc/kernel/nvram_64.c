

#include <linux/module.h>

#include <linux/types.h>
#include <linux/errno.h>
#include <linux/fs.h>
#include <linux/miscdevice.h>
#include <linux/fcntl.h>
#include <linux/nvram.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <asm/uaccess.h>
#include <asm/nvram.h>
#include <asm/rtas.h>
#include <asm/prom.h>
#include <asm/machdep.h>

#undef DEBUG_NVRAM

static struct nvram_partition * nvram_part;
static long nvram_error_log_index = -1;
static long nvram_error_log_size = 0;

struct err_log_info {
	int error_type;
	unsigned int seq_num;
};

static loff_t dev_nvram_llseek(struct file *file, loff_t offset, int origin)
{
	int size;

	if (ppc_md.nvram_size == NULL)
		return -ENODEV;
	size = ppc_md.nvram_size();

	switch (origin) {
	case 1:
		offset += file->f_pos;
		break;
	case 2:
		offset += size;
		break;
	}
	if (offset < 0)
		return -EINVAL;
	file->f_pos = offset;
	return file->f_pos;
}


static ssize_t dev_nvram_read(struct file *file, char __user *buf,
			  size_t count, loff_t *ppos)
{
	ssize_t ret;
	char *tmp = NULL;
	ssize_t size;

	ret = -ENODEV;
	if (!ppc_md.nvram_size)
		goto out;

	ret = 0;
	size = ppc_md.nvram_size();
	if (*ppos >= size || size < 0)
		goto out;

	count = min_t(size_t, count, size - *ppos);
	count = min(count, PAGE_SIZE);

	ret = -ENOMEM;
	tmp = kmalloc(count, GFP_KERNEL);
	if (!tmp)
		goto out;

	ret = ppc_md.nvram_read(tmp, count, ppos);
	if (ret <= 0)
		goto out;

	if (copy_to_user(buf, tmp, ret))
		ret = -EFAULT;

out:
	kfree(tmp);
	return ret;

}

static ssize_t dev_nvram_write(struct file *file, const char __user *buf,
			  size_t count, loff_t *ppos)
{
	ssize_t ret;
	char *tmp = NULL;
	ssize_t size;

	ret = -ENODEV;
	if (!ppc_md.nvram_size)
		goto out;

	ret = 0;
	size = ppc_md.nvram_size();
	if (*ppos >= size || size < 0)
		goto out;

	count = min_t(size_t, count, size - *ppos);
	count = min(count, PAGE_SIZE);

	ret = -ENOMEM;
	tmp = kmalloc(count, GFP_KERNEL);
	if (!tmp)
		goto out;

	ret = -EFAULT;
	if (copy_from_user(tmp, buf, count))
		goto out;

	ret = ppc_md.nvram_write(tmp, count, ppos);

out:
	kfree(tmp);
	return ret;

}

static int dev_nvram_ioctl(struct inode *inode, struct file *file,
	unsigned int cmd, unsigned long arg)
{
	switch(cmd) {
#ifdef CONFIG_PPC_PMAC
	case OBSOLETE_PMAC_NVRAM_GET_OFFSET:
		printk(KERN_WARNING "nvram: Using obsolete PMAC_NVRAM_GET_OFFSET ioctl\n");
	case IOC_NVRAM_GET_OFFSET: {
		int part, offset;

		if (!machine_is(powermac))
			return -EINVAL;
		if (copy_from_user(&part, (void __user*)arg, sizeof(part)) != 0)
			return -EFAULT;
		if (part < pmac_nvram_OF || part > pmac_nvram_NR)
			return -EINVAL;
		offset = pmac_get_partition(part);
		if (offset < 0)
			return offset;
		if (copy_to_user((void __user*)arg, &offset, sizeof(offset)) != 0)
			return -EFAULT;
		return 0;
	}
#endif /* CONFIG_PPC_PMAC */
	default:
		return -EINVAL;
	}
}

const struct file_operations nvram_fops = {
	.owner =	THIS_MODULE,
	.llseek =	dev_nvram_llseek,
	.read =		dev_nvram_read,
	.write =	dev_nvram_write,
	.ioctl =	dev_nvram_ioctl,
};

static struct miscdevice nvram_dev = {
	NVRAM_MINOR,
	"nvram",
	&nvram_fops
};


#ifdef DEBUG_NVRAM
static void nvram_print_partitions(char * label)
{
	struct list_head * p;
	struct nvram_partition * tmp_part;
	
	printk(KERN_WARNING "--------%s---------\n", label);
	printk(KERN_WARNING "indx\t\tsig\tchks\tlen\tname\n");
	list_for_each(p, &nvram_part->partition) {
		tmp_part = list_entry(p, struct nvram_partition, partition);
		printk(KERN_WARNING "%4d    \t%02x\t%02x\t%d\t%s\n",
		       tmp_part->index, tmp_part->header.signature,
		       tmp_part->header.checksum, tmp_part->header.length,
		       tmp_part->header.name);
	}
}
#endif


static int nvram_write_header(struct nvram_partition * part)
{
	loff_t tmp_index;
	int rc;
	
	tmp_index = part->index;
	rc = ppc_md.nvram_write((char *)&part->header, NVRAM_HEADER_LEN, &tmp_index); 

	return rc;
}


static unsigned char nvram_checksum(struct nvram_header *p)
{
	unsigned int c_sum, c_sum2;
	unsigned short *sp = (unsigned short *)p->name; /* assume 6 shorts */
	c_sum = p->signature + p->length + sp[0] + sp[1] + sp[2] + sp[3] + sp[4] + sp[5];

	/* The sum may have spilled into the 3rd byte.  Fold it back. */
	c_sum = ((c_sum & 0xffff) + (c_sum >> 16)) & 0xffff;
	/* The sum cannot exceed 2 bytes.  Fold it into a checksum */
	c_sum2 = (c_sum >> 8) + (c_sum << 8);
	c_sum = ((c_sum + c_sum2) >> 8) & 0xff;
	return c_sum;
}


struct nvram_partition *nvram_find_partition(int sig, const char *name)
{
	struct nvram_partition * part;
	struct list_head * p;

	list_for_each(p, &nvram_part->partition) {
		part = list_entry(p, struct nvram_partition, partition);

		if (sig && part->header.signature != sig)
			continue;
		if (name && 0 != strncmp(name, part->header.name, 12))
			continue;
		return part; 
	}
	return NULL;
}
EXPORT_SYMBOL(nvram_find_partition);


static int nvram_remove_os_partition(void)
{
	struct list_head *i;
	struct list_head *j;
	struct nvram_partition * part;
	struct nvram_partition * cur_part;
	int rc;

	list_for_each(i, &nvram_part->partition) {
		part = list_entry(i, struct nvram_partition, partition);
		if (part->header.signature != NVRAM_SIG_OS)
			continue;
		
		/* Make os partition a free partition */
		part->header.signature = NVRAM_SIG_FREE;
		sprintf(part->header.name, "wwwwwwwwwwww");
		part->header.checksum = nvram_checksum(&part->header);

		/* Merge contiguous free partitions backwards */
		list_for_each_prev(j, &part->partition) {
			cur_part = list_entry(j, struct nvram_partition, partition);
			if (cur_part == nvram_part || cur_part->header.signature != NVRAM_SIG_FREE) {
				break;
			}
			
			part->header.length += cur_part->header.length;
			part->header.checksum = nvram_checksum(&part->header);
			part->index = cur_part->index;

			list_del(&cur_part->partition);
			kfree(cur_part);
			j = &part->partition; /* fixup our loop */
		}
		
		/* Merge contiguous free partitions forwards */
		list_for_each(j, &part->partition) {
			cur_part = list_entry(j, struct nvram_partition, partition);
			if (cur_part == nvram_part || cur_part->header.signature != NVRAM_SIG_FREE) {
				break;
			}

			part->header.length += cur_part->header.length;
			part->header.checksum = nvram_checksum(&part->header);

			list_del(&cur_part->partition);
			kfree(cur_part);
			j = &part->partition; /* fixup our loop */
		}
		
		rc = nvram_write_header(part);
		if (rc <= 0) {
			printk(KERN_ERR "nvram_remove_os_partition: nvram_write failed (%d)\n", rc);
			return rc;
		}

	}
	
	return 0;
}

static int nvram_create_os_partition(void)
{
	struct nvram_partition *part;
	struct nvram_partition *new_part;
	struct nvram_partition *free_part = NULL;
	int seq_init[2] = { 0, 0 };
	loff_t tmp_index;
	long size = 0;
	int rc;
	
	/* Find a free partition that will give us the maximum needed size 
	   If can't find one that will give us the minimum size needed */
	list_for_each_entry(part, &nvram_part->partition, partition) {
		if (part->header.signature != NVRAM_SIG_FREE)
			continue;

		if (part->header.length >= NVRAM_MAX_REQ) {
			size = NVRAM_MAX_REQ;
			free_part = part;
			break;
		}
		if (!size && part->header.length >= NVRAM_MIN_REQ) {
			size = NVRAM_MIN_REQ;
			free_part = part;
		}
	}
	if (!size)
		return -ENOSPC;
	
	/* Create our OS partition */
	new_part = kmalloc(sizeof(*new_part), GFP_KERNEL);
	if (!new_part) {
		printk(KERN_ERR "nvram_create_os_partition: kmalloc failed\n");
		return -ENOMEM;
	}

	new_part->index = free_part->index;
	new_part->header.signature = NVRAM_SIG_OS;
	new_part->header.length = size;
	strcpy(new_part->header.name, "ppc64,linux");
	new_part->header.checksum = nvram_checksum(&new_part->header);

	rc = nvram_write_header(new_part);
	if (rc <= 0) {
		printk(KERN_ERR "nvram_create_os_partition: nvram_write_header \
				failed (%d)\n", rc);
		return rc;
	}

	/* make sure and initialize to zero the sequence number and the error
	   type logged */
	tmp_index = new_part->index + NVRAM_HEADER_LEN;
	rc = ppc_md.nvram_write((char *)&seq_init, sizeof(seq_init), &tmp_index);
	if (rc <= 0) {
		printk(KERN_ERR "nvram_create_os_partition: nvram_write "
				"failed (%d)\n", rc);
		return rc;
	}
	
	nvram_error_log_index = new_part->index + NVRAM_HEADER_LEN;
	nvram_error_log_size = ((part->header.length - 1) *
				NVRAM_BLOCK_LEN) - sizeof(struct err_log_info);
	
	list_add_tail(&new_part->partition, &free_part->partition);

	if (free_part->header.length <= size) {
		list_del(&free_part->partition);
		kfree(free_part);
		return 0;
	} 

	/* Adjust the partition we stole the space from */
	free_part->index += size * NVRAM_BLOCK_LEN;
	free_part->header.length -= size;
	free_part->header.checksum = nvram_checksum(&free_part->header);
	
	rc = nvram_write_header(free_part);
	if (rc <= 0) {
		printk(KERN_ERR "nvram_create_os_partition: nvram_write_header "
		       "failed (%d)\n", rc);
		return rc;
	}

	return 0;
}


static int nvram_setup_partition(void)
{
	struct list_head * p;
	struct nvram_partition * part;
	int rc;

	/* For now, we don't do any of this on pmac, until I
	 * have figured out if it's worth killing some unused stuffs
	 * in our nvram, as Apple defined partitions use pretty much
	 * all of the space
	 */
	if (machine_is(powermac))
		return -ENOSPC;

	/* see if we have an OS partition that meets our needs.
	   will try getting the max we need.  If not we'll delete
	   partitions and try again. */
	list_for_each(p, &nvram_part->partition) {
		part = list_entry(p, struct nvram_partition, partition);
		if (part->header.signature != NVRAM_SIG_OS)
			continue;

		if (strcmp(part->header.name, "ppc64,linux"))
			continue;

		if (part->header.length >= NVRAM_MIN_REQ) {
			/* found our partition */
			nvram_error_log_index = part->index + NVRAM_HEADER_LEN;
			nvram_error_log_size = ((part->header.length - 1) *
						NVRAM_BLOCK_LEN) - sizeof(struct err_log_info);
			return 0;
		}
	}
	
	/* try creating a partition with the free space we have */
	rc = nvram_create_os_partition();
	if (!rc) {
		return 0;
	}
		
	/* need to free up some space */
	rc = nvram_remove_os_partition();
	if (rc) {
		return rc;
	}
	
	/* create a partition in this new space */
	rc = nvram_create_os_partition();
	if (rc) {
		printk(KERN_ERR "nvram_create_os_partition: Could not find a "
		       "NVRAM partition large enough\n");
		return rc;
	}
	
	return 0;
}


static int nvram_scan_partitions(void)
{
	loff_t cur_index = 0;
	struct nvram_header phead;
	struct nvram_partition * tmp_part;
	unsigned char c_sum;
	char * header;
	int total_size;
	int err;

	if (ppc_md.nvram_size == NULL)
		return -ENODEV;
	total_size = ppc_md.nvram_size();
	
	header = kmalloc(NVRAM_HEADER_LEN, GFP_KERNEL);
	if (!header) {
		printk(KERN_ERR "nvram_scan_partitions: Failed kmalloc\n");
		return -ENOMEM;
	}

	while (cur_index < total_size) {

		err = ppc_md.nvram_read(header, NVRAM_HEADER_LEN, &cur_index);
		if (err != NVRAM_HEADER_LEN) {
			printk(KERN_ERR "nvram_scan_partitions: Error parsing "
			       "nvram partitions\n");
			goto out;
		}

		cur_index -= NVRAM_HEADER_LEN; /* nvram_read will advance us */

		memcpy(&phead, header, NVRAM_HEADER_LEN);

		err = 0;
		c_sum = nvram_checksum(&phead);
		if (c_sum != phead.checksum) {
			printk(KERN_WARNING "WARNING: nvram partition checksum"
			       " was %02x, should be %02x!\n",
			       phead.checksum, c_sum);
			printk(KERN_WARNING "Terminating nvram partition scan\n");
			goto out;
		}
		if (!phead.length) {
			printk(KERN_WARNING "WARNING: nvram corruption "
			       "detected: 0-length partition\n");
			goto out;
		}
		tmp_part = (struct nvram_partition *)
			kmalloc(sizeof(struct nvram_partition), GFP_KERNEL);
		err = -ENOMEM;
		if (!tmp_part) {
			printk(KERN_ERR "nvram_scan_partitions: kmalloc failed\n");
			goto out;
		}
		
		memcpy(&tmp_part->header, &phead, NVRAM_HEADER_LEN);
		tmp_part->index = cur_index;
		list_add_tail(&tmp_part->partition, &nvram_part->partition);
		
		cur_index += phead.length * NVRAM_BLOCK_LEN;
	}
	err = 0;

 out:
	kfree(header);
	return err;
}

static int __init nvram_init(void)
{
	int error;
	int rc;
	
	if (ppc_md.nvram_size == NULL || ppc_md.nvram_size() <= 0)
		return  -ENODEV;

  	rc = misc_register(&nvram_dev);
	if (rc != 0) {
		printk(KERN_ERR "nvram_init: failed to register device\n");
		return rc;
	}
  	
  	/* initialize our anchor for the nvram partition list */
  	nvram_part = kmalloc(sizeof(struct nvram_partition), GFP_KERNEL);
  	if (!nvram_part) {
  		printk(KERN_ERR "nvram_init: Failed kmalloc\n");
  		return -ENOMEM;
  	}
  	INIT_LIST_HEAD(&nvram_part->partition);
  
  	/* Get all the NVRAM partitions */
  	error = nvram_scan_partitions();
  	if (error) {
  		printk(KERN_ERR "nvram_init: Failed nvram_scan_partitions\n");
  		return error;
  	}
  		
  	if(nvram_setup_partition()) 
  		printk(KERN_WARNING "nvram_init: Could not find nvram partition"
  		       " for nvram buffered error logging.\n");
  
#ifdef DEBUG_NVRAM
	nvram_print_partitions("NVRAM Partitions");
#endif

  	return rc;
}

void __exit nvram_cleanup(void)
{
        misc_deregister( &nvram_dev );
}


#ifdef CONFIG_PPC_PSERIES

int nvram_write_error_log(char * buff, int length,
                          unsigned int err_type, unsigned int error_log_cnt)
{
	int rc;
	loff_t tmp_index;
	struct err_log_info info;
	
	if (nvram_error_log_index == -1) {
		return -ESPIPE;
	}

	if (length > nvram_error_log_size) {
		length = nvram_error_log_size;
	}

	info.error_type = err_type;
	info.seq_num = error_log_cnt;

	tmp_index = nvram_error_log_index;

	rc = ppc_md.nvram_write((char *)&info, sizeof(struct err_log_info), &tmp_index);
	if (rc <= 0) {
		printk(KERN_ERR "nvram_write_error_log: Failed nvram_write (%d)\n", rc);
		return rc;
	}

	rc = ppc_md.nvram_write(buff, length, &tmp_index);
	if (rc <= 0) {
		printk(KERN_ERR "nvram_write_error_log: Failed nvram_write (%d)\n", rc);
		return rc;
	}
	
	return 0;
}

int nvram_read_error_log(char * buff, int length,
                         unsigned int * err_type, unsigned int * error_log_cnt)
{
	int rc;
	loff_t tmp_index;
	struct err_log_info info;
	
	if (nvram_error_log_index == -1)
		return -1;

	if (length > nvram_error_log_size)
		length = nvram_error_log_size;

	tmp_index = nvram_error_log_index;

	rc = ppc_md.nvram_read((char *)&info, sizeof(struct err_log_info), &tmp_index);
	if (rc <= 0) {
		printk(KERN_ERR "nvram_read_error_log: Failed nvram_read (%d)\n", rc);
		return rc;
	}

	rc = ppc_md.nvram_read(buff, length, &tmp_index);
	if (rc <= 0) {
		printk(KERN_ERR "nvram_read_error_log: Failed nvram_read (%d)\n", rc);
		return rc;
	}

	*error_log_cnt = info.seq_num;
	*err_type = info.error_type;

	return 0;
}

int nvram_clear_error_log(void)
{
	loff_t tmp_index;
	int clear_word = ERR_FLAG_ALREADY_LOGGED;
	int rc;

	tmp_index = nvram_error_log_index;
	
	rc = ppc_md.nvram_write((char *)&clear_word, sizeof(int), &tmp_index);
	if (rc <= 0) {
		printk(KERN_ERR "nvram_clear_error_log: Failed nvram_write (%d)\n", rc);
		return rc;
	}

	return 0;
}

#endif /* CONFIG_PPC_PSERIES */

module_init(nvram_init);
module_exit(nvram_cleanup);
MODULE_LICENSE("GPL");
