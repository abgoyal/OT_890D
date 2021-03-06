

#include <linux/module.h>
#include <linux/init.h>
#include <linux/of.h>
#include <linux/mtd/mtd.h>
#include <linux/mtd/partitions.h>

int __devinit of_mtd_parse_partitions(struct device *dev,
                                      struct device_node *node,
                                      struct mtd_partition **pparts)
{
	const char *partname;
	struct device_node *pp;
	int nr_parts, i;

	/* First count the subnodes */
	pp = NULL;
	nr_parts = 0;
	while ((pp = of_get_next_child(node, pp)))
		nr_parts++;

	if (nr_parts == 0)
		return 0;

	*pparts = kzalloc(nr_parts * sizeof(**pparts), GFP_KERNEL);
	if (!*pparts)
		return -ENOMEM;

	pp = NULL;
	i = 0;
	while ((pp = of_get_next_child(node, pp))) {
		const u32 *reg;
		int len;

		reg = of_get_property(pp, "reg", &len);
		if (!reg || (len != 2 * sizeof(u32))) {
			of_node_put(pp);
			dev_err(dev, "Invalid 'reg' on %s\n", node->full_name);
			kfree(*pparts);
			*pparts = NULL;
			return -EINVAL;
		}
		(*pparts)[i].offset = reg[0];
		(*pparts)[i].size = reg[1];

		partname = of_get_property(pp, "label", &len);
		if (!partname)
			partname = of_get_property(pp, "name", &len);
		(*pparts)[i].name = (char *)partname;

		if (of_get_property(pp, "read-only", &len))
			(*pparts)[i].mask_flags = MTD_WRITEABLE;

		i++;
	}

	return nr_parts;
}
EXPORT_SYMBOL(of_mtd_parse_partitions);

MODULE_LICENSE("GPL");
