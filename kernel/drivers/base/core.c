

#include <linux/device.h>
#include <linux/err.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/kdev_t.h>
#include <linux/notifier.h>
#include <linux/genhd.h>
#include <linux/kallsyms.h>
#include <linux/semaphore.h>
#include <linux/mutex.h>

#include "base.h"
#include "power/power.h"

int (*platform_notify)(struct device *dev) = NULL;
int (*platform_notify_remove)(struct device *dev) = NULL;
static struct kobject *dev_kobj;
struct kobject *sysfs_dev_char_kobj;
struct kobject *sysfs_dev_block_kobj;

#ifdef CONFIG_BLOCK
static inline int device_is_not_partition(struct device *dev)
{
	return !(dev->type == &part_type);
}
#else
static inline int device_is_not_partition(struct device *dev)
{
	return 1;
}
#endif

const char *dev_driver_string(const struct device *dev)
{
	return dev->driver ? dev->driver->name :
			(dev->bus ? dev->bus->name :
			(dev->class ? dev->class->name : ""));
}
EXPORT_SYMBOL(dev_driver_string);

#define to_dev(obj) container_of(obj, struct device, kobj)
#define to_dev_attr(_attr) container_of(_attr, struct device_attribute, attr)

static ssize_t dev_attr_show(struct kobject *kobj, struct attribute *attr,
			     char *buf)
{
	struct device_attribute *dev_attr = to_dev_attr(attr);
	struct device *dev = to_dev(kobj);
	ssize_t ret = -EIO;

	if (dev_attr->show)
		ret = dev_attr->show(dev, dev_attr, buf);
	if (ret >= (ssize_t)PAGE_SIZE) {
		print_symbol("dev_attr_show: %s returned bad count\n",
				(unsigned long)dev_attr->show);
	}
	return ret;
}

static ssize_t dev_attr_store(struct kobject *kobj, struct attribute *attr,
			      const char *buf, size_t count)
{
	struct device_attribute *dev_attr = to_dev_attr(attr);
	struct device *dev = to_dev(kobj);
	ssize_t ret = -EIO;

	if (dev_attr->store)
		ret = dev_attr->store(dev, dev_attr, buf, count);
	return ret;
}

static struct sysfs_ops dev_sysfs_ops = {
	.show	= dev_attr_show,
	.store	= dev_attr_store,
};


static void device_release(struct kobject *kobj)
{
	struct device *dev = to_dev(kobj);

	if (dev->release)
		dev->release(dev);
	else if (dev->type && dev->type->release)
		dev->type->release(dev);
	else if (dev->class && dev->class->dev_release)
		dev->class->dev_release(dev);
	else
		WARN(1, KERN_ERR "Device '%s' does not have a release() "
			"function, it is broken and must be fixed.\n",
			dev_name(dev));
}

static struct kobj_type device_ktype = {
	.release	= device_release,
	.sysfs_ops	= &dev_sysfs_ops,
};


static int dev_uevent_filter(struct kset *kset, struct kobject *kobj)
{
	struct kobj_type *ktype = get_ktype(kobj);

	if (ktype == &device_ktype) {
		struct device *dev = to_dev(kobj);
		if (dev->uevent_suppress)
			return 0;
		if (dev->bus)
			return 1;
		if (dev->class)
			return 1;
	}
	return 0;
}

static const char *dev_uevent_name(struct kset *kset, struct kobject *kobj)
{
	struct device *dev = to_dev(kobj);

	if (dev->bus)
		return dev->bus->name;
	if (dev->class)
		return dev->class->name;
	return NULL;
}

static int dev_uevent(struct kset *kset, struct kobject *kobj,
		      struct kobj_uevent_env *env)
{
	struct device *dev = to_dev(kobj);
	int retval = 0;

	/* add the major/minor if present */
	if (MAJOR(dev->devt)) {
		add_uevent_var(env, "MAJOR=%u", MAJOR(dev->devt));
		add_uevent_var(env, "MINOR=%u", MINOR(dev->devt));
	}

	if (dev->type && dev->type->name)
		add_uevent_var(env, "DEVTYPE=%s", dev->type->name);

	if (dev->driver)
		add_uevent_var(env, "DRIVER=%s", dev->driver->name);

#ifdef CONFIG_SYSFS_DEPRECATED
	if (dev->class) {
		struct device *parent = dev->parent;

		/* find first bus device in parent chain */
		while (parent && !parent->bus)
			parent = parent->parent;
		if (parent && parent->bus) {
			const char *path;

			path = kobject_get_path(&parent->kobj, GFP_KERNEL);
			if (path) {
				add_uevent_var(env, "PHYSDEVPATH=%s", path);
				kfree(path);
			}

			add_uevent_var(env, "PHYSDEVBUS=%s", parent->bus->name);

			if (parent->driver)
				add_uevent_var(env, "PHYSDEVDRIVER=%s",
					       parent->driver->name);
		}
	} else if (dev->bus) {
		add_uevent_var(env, "PHYSDEVBUS=%s", dev->bus->name);

		if (dev->driver)
			add_uevent_var(env, "PHYSDEVDRIVER=%s",
				       dev->driver->name);
	}
#endif

	/* have the bus specific function add its stuff */
	if (dev->bus && dev->bus->uevent) {
		retval = dev->bus->uevent(dev, env);
		if (retval)
			pr_debug("device: '%s': %s: bus uevent() returned %d\n",
				 dev_name(dev), __func__, retval);
	}

	/* have the class specific function add its stuff */
	if (dev->class && dev->class->dev_uevent) {
		retval = dev->class->dev_uevent(dev, env);
		if (retval)
			pr_debug("device: '%s': %s: class uevent() "
				 "returned %d\n", dev_name(dev),
				 __func__, retval);
	}

	/* have the device type specific fuction add its stuff */
	if (dev->type && dev->type->uevent) {
		retval = dev->type->uevent(dev, env);
		if (retval)
			pr_debug("device: '%s': %s: dev_type uevent() "
				 "returned %d\n", dev_name(dev),
				 __func__, retval);
	}

	return retval;
}

static struct kset_uevent_ops device_uevent_ops = {
	.filter =	dev_uevent_filter,
	.name =		dev_uevent_name,
	.uevent =	dev_uevent,
};

static ssize_t show_uevent(struct device *dev, struct device_attribute *attr,
			   char *buf)
{
	struct kobject *top_kobj;
	struct kset *kset;
	struct kobj_uevent_env *env = NULL;
	int i;
	size_t count = 0;
	int retval;

	/* search the kset, the device belongs to */
	top_kobj = &dev->kobj;
	while (!top_kobj->kset && top_kobj->parent)
		top_kobj = top_kobj->parent;
	if (!top_kobj->kset)
		goto out;

	kset = top_kobj->kset;
	if (!kset->uevent_ops || !kset->uevent_ops->uevent)
		goto out;

	/* respect filter */
	if (kset->uevent_ops && kset->uevent_ops->filter)
		if (!kset->uevent_ops->filter(kset, &dev->kobj))
			goto out;

	env = kzalloc(sizeof(struct kobj_uevent_env), GFP_KERNEL);
	if (!env)
		return -ENOMEM;

	/* let the kset specific function add its keys */
	retval = kset->uevent_ops->uevent(kset, &dev->kobj, env);
	if (retval)
		goto out;

	/* copy keys to file */
	for (i = 0; i < env->envp_idx; i++)
		count += sprintf(&buf[count], "%s\n", env->envp[i]);
out:
	kfree(env);
	return count;
}

static ssize_t store_uevent(struct device *dev, struct device_attribute *attr,
			    const char *buf, size_t count)
{
	enum kobject_action action;

	if (kobject_action_type(buf, count, &action) == 0) {
		kobject_uevent(&dev->kobj, action);
		goto out;
	}

	dev_err(dev, "uevent: unsupported action-string; this will "
		     "be ignored in a future kernel version\n");
	kobject_uevent(&dev->kobj, KOBJ_ADD);
out:
	return count;
}

static struct device_attribute uevent_attr =
	__ATTR(uevent, S_IRUGO | S_IWUSR, show_uevent, store_uevent);

static int device_add_attributes(struct device *dev,
				 struct device_attribute *attrs)
{
	int error = 0;
	int i;

	if (attrs) {
		for (i = 0; attr_name(attrs[i]); i++) {
			error = device_create_file(dev, &attrs[i]);
			if (error)
				break;
		}
		if (error)
			while (--i >= 0)
				device_remove_file(dev, &attrs[i]);
	}
	return error;
}

static void device_remove_attributes(struct device *dev,
				     struct device_attribute *attrs)
{
	int i;

	if (attrs)
		for (i = 0; attr_name(attrs[i]); i++)
			device_remove_file(dev, &attrs[i]);
}

static int device_add_groups(struct device *dev,
			     struct attribute_group **groups)
{
	int error = 0;
	int i;

	if (groups) {
		for (i = 0; groups[i]; i++) {
			error = sysfs_create_group(&dev->kobj, groups[i]);
			if (error) {
				while (--i >= 0)
					sysfs_remove_group(&dev->kobj,
							   groups[i]);
				break;
			}
		}
	}
	return error;
}

static void device_remove_groups(struct device *dev,
				 struct attribute_group **groups)
{
	int i;

	if (groups)
		for (i = 0; groups[i]; i++)
			sysfs_remove_group(&dev->kobj, groups[i]);
}

static int device_add_attrs(struct device *dev)
{
	struct class *class = dev->class;
	struct device_type *type = dev->type;
	int error;

	if (class) {
		error = device_add_attributes(dev, class->dev_attrs);
		if (error)
			return error;
	}

	if (type) {
		error = device_add_groups(dev, type->groups);
		if (error)
			goto err_remove_class_attrs;
	}

	error = device_add_groups(dev, dev->groups);
	if (error)
		goto err_remove_type_groups;

	return 0;

 err_remove_type_groups:
	if (type)
		device_remove_groups(dev, type->groups);
 err_remove_class_attrs:
	if (class)
		device_remove_attributes(dev, class->dev_attrs);

	return error;
}

static void device_remove_attrs(struct device *dev)
{
	struct class *class = dev->class;
	struct device_type *type = dev->type;

	device_remove_groups(dev, dev->groups);

	if (type)
		device_remove_groups(dev, type->groups);

	if (class)
		device_remove_attributes(dev, class->dev_attrs);
}


static ssize_t show_dev(struct device *dev, struct device_attribute *attr,
			char *buf)
{
	return print_dev_t(buf, dev->devt);
}

static struct device_attribute devt_attr =
	__ATTR(dev, S_IRUGO, show_dev, NULL);

/* kset to create /sys/devices/  */
struct kset *devices_kset;

int device_create_file(struct device *dev, struct device_attribute *attr)
{
	int error = 0;
	if (dev)
		error = sysfs_create_file(&dev->kobj, &attr->attr);
	return error;
}

void device_remove_file(struct device *dev, struct device_attribute *attr)
{
	if (dev)
		sysfs_remove_file(&dev->kobj, &attr->attr);
}

int device_create_bin_file(struct device *dev, struct bin_attribute *attr)
{
	int error = -EINVAL;
	if (dev)
		error = sysfs_create_bin_file(&dev->kobj, attr);
	return error;
}
EXPORT_SYMBOL_GPL(device_create_bin_file);

void device_remove_bin_file(struct device *dev, struct bin_attribute *attr)
{
	if (dev)
		sysfs_remove_bin_file(&dev->kobj, attr);
}
EXPORT_SYMBOL_GPL(device_remove_bin_file);

int device_schedule_callback_owner(struct device *dev,
		void (*func)(struct device *), struct module *owner)
{
	return sysfs_schedule_callback(&dev->kobj,
			(void (*)(void *)) func, dev, owner);
}
EXPORT_SYMBOL_GPL(device_schedule_callback_owner);

static void klist_children_get(struct klist_node *n)
{
	struct device *dev = container_of(n, struct device, knode_parent);

	get_device(dev);
}

static void klist_children_put(struct klist_node *n)
{
	struct device *dev = container_of(n, struct device, knode_parent);

	put_device(dev);
}

void device_initialize(struct device *dev)
{
	dev->kobj.kset = devices_kset;
	kobject_init(&dev->kobj, &device_ktype);
	klist_init(&dev->klist_children, klist_children_get,
		   klist_children_put);
	INIT_LIST_HEAD(&dev->dma_pools);
	init_MUTEX(&dev->sem);
	spin_lock_init(&dev->devres_lock);
	INIT_LIST_HEAD(&dev->devres_head);
	device_init_wakeup(dev, 0);
	device_pm_init(dev);
	set_dev_node(dev, -1);
}

#ifdef CONFIG_SYSFS_DEPRECATED
static struct kobject *get_device_parent(struct device *dev,
					 struct device *parent)
{
	/* class devices without a parent live in /sys/class/<classname>/ */
	if (dev->class && (!parent || parent->class != dev->class))
		return &dev->class->p->class_subsys.kobj;
	/* all other devices keep their parent */
	else if (parent)
		return &parent->kobj;

	return NULL;
}

static inline void cleanup_device_parent(struct device *dev) {}
static inline void cleanup_glue_dir(struct device *dev,
				    struct kobject *glue_dir) {}
#else
static struct kobject *virtual_device_parent(struct device *dev)
{
	static struct kobject *virtual_dir = NULL;

	if (!virtual_dir)
		virtual_dir = kobject_create_and_add("virtual",
						     &devices_kset->kobj);

	return virtual_dir;
}

static struct kobject *get_device_parent(struct device *dev,
					 struct device *parent)
{
	int retval;

	if (dev->class) {
		struct kobject *kobj = NULL;
		struct kobject *parent_kobj;
		struct kobject *k;

		/*
		 * If we have no parent, we live in "virtual".
		 * Class-devices with a non class-device as parent, live
		 * in a "glue" directory to prevent namespace collisions.
		 */
		if (parent == NULL)
			parent_kobj = virtual_device_parent(dev);
		else if (parent->class)
			return &parent->kobj;
		else
			parent_kobj = &parent->kobj;

		/* find our class-directory at the parent and reference it */
		spin_lock(&dev->class->p->class_dirs.list_lock);
		list_for_each_entry(k, &dev->class->p->class_dirs.list, entry)
			if (k->parent == parent_kobj) {
				kobj = kobject_get(k);
				break;
			}
		spin_unlock(&dev->class->p->class_dirs.list_lock);
		if (kobj)
			return kobj;

		/* or create a new class-directory at the parent device */
		k = kobject_create();
		if (!k)
			return NULL;
		k->kset = &dev->class->p->class_dirs;
		retval = kobject_add(k, parent_kobj, "%s", dev->class->name);
		if (retval < 0) {
			kobject_put(k);
			return NULL;
		}
		/* do not emit an uevent for this simple "glue" directory */
		return k;
	}

	if (parent)
		return &parent->kobj;
	return NULL;
}

static void cleanup_glue_dir(struct device *dev, struct kobject *glue_dir)
{
	/* see if we live in a "glue" directory */
	if (!glue_dir || !dev->class ||
	    glue_dir->kset != &dev->class->p->class_dirs)
		return;

	kobject_put(glue_dir);
}

static void cleanup_device_parent(struct device *dev)
{
	cleanup_glue_dir(dev, dev->kobj.parent);
}
#endif

static void setup_parent(struct device *dev, struct device *parent)
{
	struct kobject *kobj;
	kobj = get_device_parent(dev, parent);
	if (kobj)
		dev->kobj.parent = kobj;
}

static int device_add_class_symlinks(struct device *dev)
{
	int error;

	if (!dev->class)
		return 0;

	error = sysfs_create_link(&dev->kobj,
				  &dev->class->p->class_subsys.kobj,
				  "subsystem");
	if (error)
		goto out;

#ifdef CONFIG_SYSFS_DEPRECATED
	/* stacked class devices need a symlink in the class directory */
	if (dev->kobj.parent != &dev->class->p->class_subsys.kobj &&
	    device_is_not_partition(dev)) {
		error = sysfs_create_link(&dev->class->p->class_subsys.kobj,
					  &dev->kobj, dev_name(dev));
		if (error)
			goto out_subsys;
	}

	if (dev->parent && device_is_not_partition(dev)) {
		struct device *parent = dev->parent;
		char *class_name;

		/*
		 * stacked class devices have the 'device' link
		 * pointing to the bus device instead of the parent
		 */
		while (parent->class && !parent->bus && parent->parent)
			parent = parent->parent;

		error = sysfs_create_link(&dev->kobj,
					  &parent->kobj,
					  "device");
		if (error)
			goto out_busid;

		class_name = make_class_name(dev->class->name,
						&dev->kobj);
		if (class_name)
			error = sysfs_create_link(&dev->parent->kobj,
						&dev->kobj, class_name);
		kfree(class_name);
		if (error)
			goto out_device;
	}
	return 0;

out_device:
	if (dev->parent && device_is_not_partition(dev))
		sysfs_remove_link(&dev->kobj, "device");
out_busid:
	if (dev->kobj.parent != &dev->class->p->class_subsys.kobj &&
	    device_is_not_partition(dev))
		sysfs_remove_link(&dev->class->p->class_subsys.kobj,
				  dev_name(dev));
#else
	/* link in the class directory pointing to the device */
	error = sysfs_create_link(&dev->class->p->class_subsys.kobj,
				  &dev->kobj, dev_name(dev));
	if (error)
		goto out_subsys;

	if (dev->parent && device_is_not_partition(dev)) {
		error = sysfs_create_link(&dev->kobj, &dev->parent->kobj,
					  "device");
		if (error)
			goto out_busid;
	}
	return 0;

out_busid:
	sysfs_remove_link(&dev->class->p->class_subsys.kobj, dev_name(dev));
#endif

out_subsys:
	sysfs_remove_link(&dev->kobj, "subsystem");
out:
	return error;
}

static void device_remove_class_symlinks(struct device *dev)
{
	if (!dev->class)
		return;

#ifdef CONFIG_SYSFS_DEPRECATED
	if (dev->parent && device_is_not_partition(dev)) {
		char *class_name;

		class_name = make_class_name(dev->class->name, &dev->kobj);
		if (class_name) {
			sysfs_remove_link(&dev->parent->kobj, class_name);
			kfree(class_name);
		}
		sysfs_remove_link(&dev->kobj, "device");
	}

	if (dev->kobj.parent != &dev->class->p->class_subsys.kobj &&
	    device_is_not_partition(dev))
		sysfs_remove_link(&dev->class->p->class_subsys.kobj,
				  dev_name(dev));
#else
	if (dev->parent && device_is_not_partition(dev))
		sysfs_remove_link(&dev->kobj, "device");

	sysfs_remove_link(&dev->class->p->class_subsys.kobj, dev_name(dev));
#endif

	sysfs_remove_link(&dev->kobj, "subsystem");
}

int dev_set_name(struct device *dev, const char *fmt, ...)
{
	va_list vargs;
	char *s;

	va_start(vargs, fmt);
	vsnprintf(dev->bus_id, sizeof(dev->bus_id), fmt, vargs);
	va_end(vargs);

	/* ewww... some of these buggers have / in the name... */
	while ((s = strchr(dev->bus_id, '/')))
		*s = '!';

	return 0;
}
EXPORT_SYMBOL_GPL(dev_set_name);

static struct kobject *device_to_dev_kobj(struct device *dev)
{
	struct kobject *kobj;

	if (dev->class)
		kobj = dev->class->dev_kobj;
	else
		kobj = sysfs_dev_char_kobj;

	return kobj;
}

static int device_create_sys_dev_entry(struct device *dev)
{
	struct kobject *kobj = device_to_dev_kobj(dev);
	int error = 0;
	char devt_str[15];

	if (kobj) {
		format_dev_t(devt_str, dev->devt);
		error = sysfs_create_link(kobj, &dev->kobj, devt_str);
	}

	return error;
}

static void device_remove_sys_dev_entry(struct device *dev)
{
	struct kobject *kobj = device_to_dev_kobj(dev);
	char devt_str[15];

	if (kobj) {
		format_dev_t(devt_str, dev->devt);
		sysfs_remove_link(kobj, devt_str);
	}
}

int device_add(struct device *dev)
{
	struct device *parent = NULL;
	struct class_interface *class_intf;
	int error = -EINVAL;

	dev = get_device(dev);
	if (!dev)
		goto done;

	/* Temporarily support init_name if it is set.
	 * It will override bus_id for now */
	if (dev->init_name)
		dev_set_name(dev, "%s", dev->init_name);

	if (!strlen(dev->bus_id))
		goto done;

	pr_debug("device: '%s': %s\n", dev_name(dev), __func__);

	parent = get_device(dev->parent);
	setup_parent(dev, parent);

	/* use parent numa_node */
	if (parent)
		set_dev_node(dev, dev_to_node(parent));

	/* first, register with generic layer. */
	error = kobject_add(&dev->kobj, dev->kobj.parent, "%s", dev_name(dev));
	if (error)
		goto Error;

	/* notify platform of device entry */
	if (platform_notify)
		platform_notify(dev);

	error = device_create_file(dev, &uevent_attr);
	if (error)
		goto attrError;

	if (MAJOR(dev->devt)) {
		error = device_create_file(dev, &devt_attr);
		if (error)
			goto ueventattrError;

		error = device_create_sys_dev_entry(dev);
		if (error)
			goto devtattrError;
	}

	error = device_add_class_symlinks(dev);
	if (error)
		goto SymlinkError;
	error = device_add_attrs(dev);
	if (error)
		goto AttrsError;
	error = bus_add_device(dev);
	if (error)
		goto BusError;
	error = dpm_sysfs_add(dev);
	if (error)
		goto DPMError;
	device_pm_add(dev);

	/* Notify clients of device addition.  This call must come
	 * after dpm_sysf_add() and before kobject_uevent().
	 */
	if (dev->bus)
		blocking_notifier_call_chain(&dev->bus->p->bus_notifier,
					     BUS_NOTIFY_ADD_DEVICE, dev);

	kobject_uevent(&dev->kobj, KOBJ_ADD);
	bus_attach_device(dev);
	if (parent)
		klist_add_tail(&dev->knode_parent, &parent->klist_children);

	if (dev->class) {
		mutex_lock(&dev->class->p->class_mutex);
		/* tie the class to the device */
		klist_add_tail(&dev->knode_class,
			       &dev->class->p->class_devices);

		/* notify any interfaces that the device is here */
		list_for_each_entry(class_intf,
				    &dev->class->p->class_interfaces, node)
			if (class_intf->add_dev)
				class_intf->add_dev(dev, class_intf);
		mutex_unlock(&dev->class->p->class_mutex);
	}
done:
	put_device(dev);
	return error;
 DPMError:
	bus_remove_device(dev);
 BusError:
	device_remove_attrs(dev);
 AttrsError:
	device_remove_class_symlinks(dev);
 SymlinkError:
	if (MAJOR(dev->devt))
		device_remove_sys_dev_entry(dev);
 devtattrError:
	if (MAJOR(dev->devt))
		device_remove_file(dev, &devt_attr);
 ueventattrError:
	device_remove_file(dev, &uevent_attr);
 attrError:
	kobject_uevent(&dev->kobj, KOBJ_REMOVE);
	kobject_del(&dev->kobj);
 Error:
	cleanup_device_parent(dev);
	if (parent)
		put_device(parent);
	goto done;
}

int device_register(struct device *dev)
{
	device_initialize(dev);
	return device_add(dev);
}

struct device *get_device(struct device *dev)
{
	return dev ? to_dev(kobject_get(&dev->kobj)) : NULL;
}

void put_device(struct device *dev)
{
	/* might_sleep(); */
	if (dev)
		kobject_put(&dev->kobj);
}

void device_del(struct device *dev)
{
	struct device *parent = dev->parent;
	struct class_interface *class_intf;

	/* Notify clients of device removal.  This call must come
	 * before dpm_sysfs_remove().
	 */
	if (dev->bus)
		blocking_notifier_call_chain(&dev->bus->p->bus_notifier,
					     BUS_NOTIFY_DEL_DEVICE, dev);
	device_pm_remove(dev);
	dpm_sysfs_remove(dev);
	if (parent)
		klist_del(&dev->knode_parent);
	if (MAJOR(dev->devt)) {
		device_remove_sys_dev_entry(dev);
		device_remove_file(dev, &devt_attr);
	}
	if (dev->class) {
		device_remove_class_symlinks(dev);

		mutex_lock(&dev->class->p->class_mutex);
		/* notify any interfaces that the device is now gone */
		list_for_each_entry(class_intf,
				    &dev->class->p->class_interfaces, node)
			if (class_intf->remove_dev)
				class_intf->remove_dev(dev, class_intf);
		/* remove the device from the class list */
		klist_del(&dev->knode_class);
		mutex_unlock(&dev->class->p->class_mutex);
	}
	device_remove_file(dev, &uevent_attr);
	device_remove_attrs(dev);
	bus_remove_device(dev);

	/*
	 * Some platform devices are driven without driver attached
	 * and managed resources may have been acquired.  Make sure
	 * all resources are released.
	 */
	devres_release_all(dev);

	/* Notify the platform of the removal, in case they
	 * need to do anything...
	 */
	if (platform_notify_remove)
		platform_notify_remove(dev);
	kobject_uevent(&dev->kobj, KOBJ_REMOVE);
	cleanup_device_parent(dev);
	kobject_del(&dev->kobj);
	put_device(parent);
}

void device_unregister(struct device *dev)
{
	pr_debug("device: '%s': %s\n", dev_name(dev), __func__);
	device_del(dev);
	put_device(dev);
}

static struct device *next_device(struct klist_iter *i)
{
	struct klist_node *n = klist_next(i);
	return n ? container_of(n, struct device, knode_parent) : NULL;
}

int device_for_each_child(struct device *parent, void *data,
			  int (*fn)(struct device *dev, void *data))
{
	struct klist_iter i;
	struct device *child;
	int error = 0;

	klist_iter_init(&parent->klist_children, &i);
	while ((child = next_device(&i)) && !error)
		error = fn(child, data);
	klist_iter_exit(&i);
	return error;
}

struct device *device_find_child(struct device *parent, void *data,
				 int (*match)(struct device *dev, void *data))
{
	struct klist_iter i;
	struct device *child;

	if (!parent)
		return NULL;

	klist_iter_init(&parent->klist_children, &i);
	while ((child = next_device(&i)))
		if (match(child, data) && get_device(child))
			break;
	klist_iter_exit(&i);
	return child;
}

int __init devices_init(void)
{
	devices_kset = kset_create_and_add("devices", &device_uevent_ops, NULL);
	if (!devices_kset)
		return -ENOMEM;
	dev_kobj = kobject_create_and_add("dev", NULL);
	if (!dev_kobj)
		goto dev_kobj_err;
	sysfs_dev_block_kobj = kobject_create_and_add("block", dev_kobj);
	if (!sysfs_dev_block_kobj)
		goto block_kobj_err;
	sysfs_dev_char_kobj = kobject_create_and_add("char", dev_kobj);
	if (!sysfs_dev_char_kobj)
		goto char_kobj_err;

	return 0;

 char_kobj_err:
	kobject_put(sysfs_dev_block_kobj);
 block_kobj_err:
	kobject_put(dev_kobj);
 dev_kobj_err:
	kset_unregister(devices_kset);
	return -ENOMEM;
}

EXPORT_SYMBOL_GPL(device_for_each_child);
EXPORT_SYMBOL_GPL(device_find_child);

EXPORT_SYMBOL_GPL(device_initialize);
EXPORT_SYMBOL_GPL(device_add);
EXPORT_SYMBOL_GPL(device_register);

EXPORT_SYMBOL_GPL(device_del);
EXPORT_SYMBOL_GPL(device_unregister);
EXPORT_SYMBOL_GPL(get_device);
EXPORT_SYMBOL_GPL(put_device);

EXPORT_SYMBOL_GPL(device_create_file);
EXPORT_SYMBOL_GPL(device_remove_file);

struct root_device
{
	struct device dev;
	struct module *owner;
};

#define to_root_device(dev) container_of(dev, struct root_device, dev)

static void root_device_release(struct device *dev)
{
	kfree(to_root_device(dev));
}

struct device *__root_device_register(const char *name, struct module *owner)
{
	struct root_device *root;
	int err = -ENOMEM;

	root = kzalloc(sizeof(struct root_device), GFP_KERNEL);
	if (!root)
		return ERR_PTR(err);

	err = dev_set_name(&root->dev, name);
	if (err) {
		kfree(root);
		return ERR_PTR(err);
	}

	root->dev.release = root_device_release;

	err = device_register(&root->dev);
	if (err) {
		put_device(&root->dev);
		return ERR_PTR(err);
	}

#ifdef CONFIG_MODULE	/* gotta find a "cleaner" way to do this */
	if (owner) {
		struct module_kobject *mk = &owner->mkobj;

		err = sysfs_create_link(&root->dev.kobj, &mk->kobj, "module");
		if (err) {
			device_unregister(&root->dev);
			return ERR_PTR(err);
		}
		root->owner = owner;
	}
#endif

	return &root->dev;
}
EXPORT_SYMBOL_GPL(__root_device_register);

void root_device_unregister(struct device *dev)
{
	struct root_device *root = to_root_device(dev);

	if (root->owner)
		sysfs_remove_link(&root->dev.kobj, "module");

	device_unregister(dev);
}
EXPORT_SYMBOL_GPL(root_device_unregister);


static void device_create_release(struct device *dev)
{
	pr_debug("device: '%s': %s\n", dev_name(dev), __func__);
	kfree(dev);
}

struct device *device_create_vargs(struct class *class, struct device *parent,
				   dev_t devt, void *drvdata, const char *fmt,
				   va_list args)
{
	struct device *dev = NULL;
	int retval = -ENODEV;

	if (class == NULL || IS_ERR(class))
		goto error;

	dev = kzalloc(sizeof(*dev), GFP_KERNEL);
	if (!dev) {
		retval = -ENOMEM;
		goto error;
	}

	dev->devt = devt;
	dev->class = class;
	dev->parent = parent;
	dev->release = device_create_release;
	dev_set_drvdata(dev, drvdata);

	vsnprintf(dev->bus_id, BUS_ID_SIZE, fmt, args);
	retval = device_register(dev);
	if (retval)
		goto error;

	return dev;

error:
	put_device(dev);
	return ERR_PTR(retval);
}
EXPORT_SYMBOL_GPL(device_create_vargs);

struct device *device_create(struct class *class, struct device *parent,
			     dev_t devt, void *drvdata, const char *fmt, ...)
{
	va_list vargs;
	struct device *dev;

	va_start(vargs, fmt);
	dev = device_create_vargs(class, parent, devt, drvdata, fmt, vargs);
	va_end(vargs);
	return dev;
}
EXPORT_SYMBOL_GPL(device_create);

static int __match_devt(struct device *dev, void *data)
{
	dev_t *devt = data;

	return dev->devt == *devt;
}

void device_destroy(struct class *class, dev_t devt)
{
	struct device *dev;

	dev = class_find_device(class, NULL, &devt, __match_devt);
	if (dev) {
		put_device(dev);
		device_unregister(dev);
	}
}
EXPORT_SYMBOL_GPL(device_destroy);

int device_rename(struct device *dev, char *new_name)
{
	char *old_class_name = NULL;
	char *new_class_name = NULL;
	char *old_device_name = NULL;
	int error;

	dev = get_device(dev);
	if (!dev)
		return -EINVAL;

	pr_debug("device: '%s': %s: renaming to '%s'\n", dev_name(dev),
		 __func__, new_name);

#ifdef CONFIG_SYSFS_DEPRECATED
	if ((dev->class) && (dev->parent))
		old_class_name = make_class_name(dev->class->name, &dev->kobj);
#endif

	old_device_name = kmalloc(BUS_ID_SIZE, GFP_KERNEL);
	if (!old_device_name) {
		error = -ENOMEM;
		goto out;
	}
	strlcpy(old_device_name, dev->bus_id, BUS_ID_SIZE);
	strlcpy(dev->bus_id, new_name, BUS_ID_SIZE);

	error = kobject_rename(&dev->kobj, new_name);
	if (error) {
		strlcpy(dev->bus_id, old_device_name, BUS_ID_SIZE);
		goto out;
	}

#ifdef CONFIG_SYSFS_DEPRECATED
	if (old_class_name) {
		new_class_name = make_class_name(dev->class->name, &dev->kobj);
		if (new_class_name) {
			error = sysfs_create_link_nowarn(&dev->parent->kobj,
							 &dev->kobj,
							 new_class_name);
			if (error)
				goto out;
			sysfs_remove_link(&dev->parent->kobj, old_class_name);
		}
	}
#else
	if (dev->class) {
		error = sysfs_create_link_nowarn(&dev->class->p->class_subsys.kobj,
						 &dev->kobj, dev_name(dev));
		if (error)
			goto out;
		sysfs_remove_link(&dev->class->p->class_subsys.kobj,
				  old_device_name);
	}
#endif

out:
	put_device(dev);

	kfree(new_class_name);
	kfree(old_class_name);
	kfree(old_device_name);

	return error;
}
EXPORT_SYMBOL_GPL(device_rename);

static int device_move_class_links(struct device *dev,
				   struct device *old_parent,
				   struct device *new_parent)
{
	int error = 0;
#ifdef CONFIG_SYSFS_DEPRECATED
	char *class_name;

	class_name = make_class_name(dev->class->name, &dev->kobj);
	if (!class_name) {
		error = -ENOMEM;
		goto out;
	}
	if (old_parent) {
		sysfs_remove_link(&dev->kobj, "device");
		sysfs_remove_link(&old_parent->kobj, class_name);
	}
	if (new_parent) {
		error = sysfs_create_link(&dev->kobj, &new_parent->kobj,
					  "device");
		if (error)
			goto out;
		error = sysfs_create_link(&new_parent->kobj, &dev->kobj,
					  class_name);
		if (error)
			sysfs_remove_link(&dev->kobj, "device");
	} else
		error = 0;
out:
	kfree(class_name);
	return error;
#else
	if (old_parent)
		sysfs_remove_link(&dev->kobj, "device");
	if (new_parent)
		error = sysfs_create_link(&dev->kobj, &new_parent->kobj,
					  "device");
	return error;
#endif
}

int device_move(struct device *dev, struct device *new_parent)
{
	int error;
	struct device *old_parent;
	struct kobject *new_parent_kobj;

	dev = get_device(dev);
	if (!dev)
		return -EINVAL;

	new_parent = get_device(new_parent);
	new_parent_kobj = get_device_parent(dev, new_parent);

	pr_debug("device: '%s': %s: moving to '%s'\n", dev_name(dev),
		 __func__, new_parent ? dev_name(new_parent) : "<NULL>");
	error = kobject_move(&dev->kobj, new_parent_kobj);
	if (error) {
		cleanup_glue_dir(dev, new_parent_kobj);
		put_device(new_parent);
		goto out;
	}
	old_parent = dev->parent;
	dev->parent = new_parent;
	if (old_parent)
		klist_remove(&dev->knode_parent);
	if (new_parent) {
		klist_add_tail(&dev->knode_parent, &new_parent->klist_children);
		set_dev_node(dev, dev_to_node(new_parent));
	}

	if (!dev->class)
		goto out_put;
	error = device_move_class_links(dev, old_parent, new_parent);
	if (error) {
		/* We ignore errors on cleanup since we're hosed anyway... */
		device_move_class_links(dev, new_parent, old_parent);
		if (!kobject_move(&dev->kobj, &old_parent->kobj)) {
			if (new_parent)
				klist_remove(&dev->knode_parent);
			dev->parent = old_parent;
			if (old_parent) {
				klist_add_tail(&dev->knode_parent,
					       &old_parent->klist_children);
				set_dev_node(dev, dev_to_node(old_parent));
			}
		}
		cleanup_glue_dir(dev, new_parent_kobj);
		put_device(new_parent);
		goto out;
	}
out_put:
	put_device(old_parent);
out:
	put_device(dev);
	return error;
}
EXPORT_SYMBOL_GPL(device_move);

void device_shutdown(void)
{
	struct device *dev, *devn;

	list_for_each_entry_safe_reverse(dev, devn, &devices_kset->list,
				kobj.entry) {
		if (dev->bus && dev->bus->shutdown) {
			dev_dbg(dev, "shutdown\n");
			dev->bus->shutdown(dev);
		} else if (dev->driver && dev->driver->shutdown) {
			dev_dbg(dev, "shutdown\n");
			dev->driver->shutdown(dev);
		}
	}
	kobject_put(sysfs_dev_char_kobj);
	kobject_put(sysfs_dev_block_kobj);
	kobject_put(dev_kobj);
}
