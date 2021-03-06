


#include <linux/device.h>
#include <linux/kdev_t.h>
#include <linux/err.h>

#include "drm_core.h"
#include "drmP.h"

#define to_drm_minor(d) container_of(d, struct drm_minor, kdev)
#define to_drm_connector(d) container_of(d, struct drm_connector, kdev)

static int drm_sysfs_suspend(struct device *dev, pm_message_t state)
{
	struct drm_minor *drm_minor = to_drm_minor(dev);
	struct drm_device *drm_dev = drm_minor->dev;

	if (drm_minor->type == DRM_MINOR_LEGACY && drm_dev->driver->suspend)
		return drm_dev->driver->suspend(drm_dev, state);

	return 0;
}

static int drm_sysfs_resume(struct device *dev)
{
	struct drm_minor *drm_minor = to_drm_minor(dev);
	struct drm_device *drm_dev = drm_minor->dev;

	if (drm_minor->type == DRM_MINOR_LEGACY && drm_dev->driver->resume)
		return drm_dev->driver->resume(drm_dev);

	return 0;
}

/* Display the version of drm_core. This doesn't work right in current design */
static ssize_t version_show(struct class *dev, char *buf)
{
	return sprintf(buf, "%s %d.%d.%d %s\n", CORE_NAME, CORE_MAJOR,
		       CORE_MINOR, CORE_PATCHLEVEL, CORE_DATE);
}

static CLASS_ATTR(version, S_IRUGO, version_show, NULL);

struct class *drm_sysfs_create(struct module *owner, char *name)
{
	struct class *class;
	int err;

	class = class_create(owner, name);
	if (IS_ERR(class)) {
		err = PTR_ERR(class);
		goto err_out;
	}

	class->suspend = drm_sysfs_suspend;
	class->resume = drm_sysfs_resume;

	err = class_create_file(class, &class_attr_version);
	if (err)
		goto err_out_class;

	return class;

err_out_class:
	class_destroy(class);
err_out:
	return ERR_PTR(err);
}

void drm_sysfs_destroy(void)
{
	if ((drm_class == NULL) || (IS_ERR(drm_class)))
		return;
	class_remove_file(drm_class, &class_attr_version);
	class_destroy(drm_class);
}

static ssize_t show_dri(struct device *device, struct device_attribute *attr,
			char *buf)
{
	struct drm_minor *drm_minor = to_drm_minor(device);
	struct drm_device *drm_dev = drm_minor->dev;
	if (drm_dev->driver->dri_library_name)
		return drm_dev->driver->dri_library_name(drm_dev, buf);
	return snprintf(buf, PAGE_SIZE, "%s\n", drm_dev->driver->pci_driver.name);
}

static struct device_attribute device_attrs[] = {
	__ATTR(dri_library_name, S_IRUGO, show_dri, NULL),
};

static void drm_sysfs_device_release(struct device *dev)
{
	return;
}

static ssize_t status_show(struct device *device,
			   struct device_attribute *attr,
			   char *buf)
{
	struct drm_connector *connector = to_drm_connector(device);
	enum drm_connector_status status;

	status = connector->funcs->detect(connector);
	return snprintf(buf, PAGE_SIZE, "%s",
			drm_get_connector_status_name(status));
}

static ssize_t dpms_show(struct device *device,
			   struct device_attribute *attr,
			   char *buf)
{
	struct drm_connector *connector = to_drm_connector(device);
	struct drm_device *dev = connector->dev;
	uint64_t dpms_status;
	int ret;

	ret = drm_connector_property_get_value(connector,
					    dev->mode_config.dpms_property,
					    &dpms_status);
	if (ret)
		return 0;

	return snprintf(buf, PAGE_SIZE, "%s",
			drm_get_dpms_name((int)dpms_status));
}

static ssize_t enabled_show(struct device *device,
			    struct device_attribute *attr,
			   char *buf)
{
	struct drm_connector *connector = to_drm_connector(device);

	return snprintf(buf, PAGE_SIZE, connector->encoder ? "enabled" :
			"disabled");
}

static ssize_t edid_show(struct kobject *kobj, struct bin_attribute *attr,
			 char *buf, loff_t off, size_t count)
{
	struct device *connector_dev = container_of(kobj, struct device, kobj);
	struct drm_connector *connector = to_drm_connector(connector_dev);
	unsigned char *edid;
	size_t size;

	if (!connector->edid_blob_ptr)
		return 0;

	edid = connector->edid_blob_ptr->data;
	size = connector->edid_blob_ptr->length;
	if (!edid)
		return 0;

	if (off >= size)
		return 0;

	if (off + count > size)
		count = size - off;
	memcpy(buf, edid + off, count);

	return count;
}

static ssize_t modes_show(struct device *device,
			   struct device_attribute *attr,
			   char *buf)
{
	struct drm_connector *connector = to_drm_connector(device);
	struct drm_display_mode *mode;
	int written = 0;

	list_for_each_entry(mode, &connector->modes, head) {
		written += snprintf(buf + written, PAGE_SIZE - written, "%s\n",
				    mode->name);
	}

	return written;
}

static ssize_t subconnector_show(struct device *device,
			   struct device_attribute *attr,
			   char *buf)
{
	struct drm_connector *connector = to_drm_connector(device);
	struct drm_device *dev = connector->dev;
	struct drm_property *prop = NULL;
	uint64_t subconnector;
	int is_tv = 0;
	int ret;

	switch (connector->connector_type) {
		case DRM_MODE_CONNECTOR_DVII:
			prop = dev->mode_config.dvi_i_subconnector_property;
			break;
		case DRM_MODE_CONNECTOR_Composite:
		case DRM_MODE_CONNECTOR_SVIDEO:
		case DRM_MODE_CONNECTOR_Component:
			prop = dev->mode_config.tv_subconnector_property;
			is_tv = 1;
			break;
		default:
			DRM_ERROR("Wrong connector type for this property\n");
			return 0;
	}

	if (!prop) {
		DRM_ERROR("Unable to find subconnector property\n");
		return 0;
	}

	ret = drm_connector_property_get_value(connector, prop, &subconnector);
	if (ret)
		return 0;

	return snprintf(buf, PAGE_SIZE, "%s", is_tv ?
			drm_get_tv_subconnector_name((int)subconnector) :
			drm_get_dvi_i_subconnector_name((int)subconnector));
}

static ssize_t select_subconnector_show(struct device *device,
			   struct device_attribute *attr,
			   char *buf)
{
	struct drm_connector *connector = to_drm_connector(device);
	struct drm_device *dev = connector->dev;
	struct drm_property *prop = NULL;
	uint64_t subconnector;
	int is_tv = 0;
	int ret;

	switch (connector->connector_type) {
		case DRM_MODE_CONNECTOR_DVII:
			prop = dev->mode_config.dvi_i_select_subconnector_property;
			break;
		case DRM_MODE_CONNECTOR_Composite:
		case DRM_MODE_CONNECTOR_SVIDEO:
		case DRM_MODE_CONNECTOR_Component:
			prop = dev->mode_config.tv_select_subconnector_property;
			is_tv = 1;
			break;
		default:
			DRM_ERROR("Wrong connector type for this property\n");
			return 0;
	}

	if (!prop) {
		DRM_ERROR("Unable to find select subconnector property\n");
		return 0;
	}

	ret = drm_connector_property_get_value(connector, prop, &subconnector);
	if (ret)
		return 0;

	return snprintf(buf, PAGE_SIZE, "%s", is_tv ?
			drm_get_tv_select_name((int)subconnector) :
			drm_get_dvi_i_select_name((int)subconnector));
}

static struct device_attribute connector_attrs[] = {
	__ATTR_RO(status),
	__ATTR_RO(enabled),
	__ATTR_RO(dpms),
	__ATTR_RO(modes),
};

/* These attributes are for both DVI-I connectors and all types of tv-out. */
static struct device_attribute connector_attrs_opt1[] = {
	__ATTR_RO(subconnector),
	__ATTR_RO(select_subconnector),
};

static struct bin_attribute edid_attr = {
	.attr.name = "edid",
	.size = 128,
	.read = edid_show,
};

int drm_sysfs_connector_add(struct drm_connector *connector)
{
	struct drm_device *dev = connector->dev;
	int ret = 0, i, j;

	/* We shouldn't get called more than once for the same connector */
	BUG_ON(device_is_registered(&connector->kdev));

	connector->kdev.parent = &dev->primary->kdev;
	connector->kdev.class = drm_class;
	connector->kdev.release = drm_sysfs_device_release;

	DRM_DEBUG("adding \"%s\" to sysfs\n",
		  drm_get_connector_name(connector));

	snprintf(connector->kdev.bus_id, BUS_ID_SIZE, "card%d-%s",
		 dev->primary->index, drm_get_connector_name(connector));
	ret = device_register(&connector->kdev);

	if (ret) {
		DRM_ERROR("failed to register connector device: %d\n", ret);
		goto out;
	}

	/* Standard attributes */

	for (i = 0; i < ARRAY_SIZE(connector_attrs); i++) {
		ret = device_create_file(&connector->kdev, &connector_attrs[i]);
		if (ret)
			goto err_out_files;
	}

	/* Optional attributes */
	/*
	 * In the long run it maybe a good idea to make one set of
	 * optionals per connector type.
	 */
	switch (connector->connector_type) {
		case DRM_MODE_CONNECTOR_DVII:
		case DRM_MODE_CONNECTOR_Composite:
		case DRM_MODE_CONNECTOR_SVIDEO:
		case DRM_MODE_CONNECTOR_Component:
			for (i = 0; i < ARRAY_SIZE(connector_attrs_opt1); i++) {
				ret = device_create_file(&connector->kdev, &connector_attrs_opt1[i]);
				if (ret)
					goto err_out_files;
			}
			break;
		default:
			break;
	}

	ret = sysfs_create_bin_file(&connector->kdev.kobj, &edid_attr);
	if (ret)
		goto err_out_files;

	/* Let userspace know we have a new connector */
	drm_sysfs_hotplug_event(dev);

	return 0;

err_out_files:
	if (i > 0)
		for (j = 0; j < i; j++)
			device_remove_file(&connector->kdev,
					   &connector_attrs[i]);
	device_unregister(&connector->kdev);

out:
	return ret;
}
EXPORT_SYMBOL(drm_sysfs_connector_add);

void drm_sysfs_connector_remove(struct drm_connector *connector)
{
	int i;

	DRM_DEBUG("removing \"%s\" from sysfs\n",
		  drm_get_connector_name(connector));

	for (i = 0; i < ARRAY_SIZE(connector_attrs); i++)
		device_remove_file(&connector->kdev, &connector_attrs[i]);
	sysfs_remove_bin_file(&connector->kdev.kobj, &edid_attr);
	device_unregister(&connector->kdev);
}
EXPORT_SYMBOL(drm_sysfs_connector_remove);

void drm_sysfs_hotplug_event(struct drm_device *dev)
{
	char *event_string = "HOTPLUG=1";
	char *envp[] = { event_string, NULL };

	DRM_DEBUG("generating hotplug event\n");

	kobject_uevent_env(&dev->primary->kdev.kobj, KOBJ_CHANGE, envp);
}

int drm_sysfs_device_add(struct drm_minor *minor)
{
	int err;
	int i, j;
	char *minor_str;

	minor->kdev.parent = &minor->dev->pdev->dev;
	minor->kdev.class = drm_class;
	minor->kdev.release = drm_sysfs_device_release;
	minor->kdev.devt = minor->device;
	if (minor->type == DRM_MINOR_CONTROL)
		minor_str = "controlD%d";
        else if (minor->type == DRM_MINOR_RENDER)
                minor_str = "renderD%d";
        else
                minor_str = "card%d";

	dev_set_name(&minor->kdev, minor_str, minor->index);

	err = device_register(&minor->kdev);
	if (err) {
		DRM_ERROR("device add failed: %d\n", err);
		goto err_out;
	}

	for (i = 0; i < ARRAY_SIZE(device_attrs); i++) {
		err = device_create_file(&minor->kdev, &device_attrs[i]);
		if (err)
			goto err_out_files;
	}

	return 0;

err_out_files:
	if (i > 0)
		for (j = 0; j < i; j++)
			device_remove_file(&minor->kdev, &device_attrs[j]);
	device_unregister(&minor->kdev);
err_out:

	return err;
}

void drm_sysfs_device_remove(struct drm_minor *minor)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(device_attrs); i++)
		device_remove_file(&minor->kdev, &device_attrs[i]);
	device_unregister(&minor->kdev);
}
