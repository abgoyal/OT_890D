

#include <linux/module.h>
#include <linux/cgroup.h>
#include <linux/fs.h>
#include <linux/proc_fs.h>
#include <linux/slab.h>
#include <linux/nsproxy.h>

struct ns_cgroup {
	struct cgroup_subsys_state css;
};

struct cgroup_subsys ns_subsys;

static inline struct ns_cgroup *cgroup_to_ns(
		struct cgroup *cgroup)
{
	return container_of(cgroup_subsys_state(cgroup, ns_subsys_id),
			    struct ns_cgroup, css);
}

int ns_cgroup_clone(struct task_struct *task, struct pid *pid)
{
	char name[PROC_NUMBUF];

	snprintf(name, PROC_NUMBUF, "%d", pid_vnr(pid));
	return cgroup_clone(task, &ns_subsys, name);
}

static int ns_can_attach(struct cgroup_subsys *ss,
		struct cgroup *new_cgroup, struct task_struct *task)
{
	struct cgroup *orig;

	if (current != task) {
		if (!capable(CAP_SYS_ADMIN))
			return -EPERM;

		if (!cgroup_is_descendant(new_cgroup))
			return -EPERM;
	}

	if (atomic_read(&new_cgroup->count) != 0)
		return -EPERM;

	orig = task_cgroup(task, ns_subsys_id);
	if (orig && orig != new_cgroup->parent)
		return -EPERM;

	return 0;
}

static struct cgroup_subsys_state *ns_create(struct cgroup_subsys *ss,
						struct cgroup *cgroup)
{
	struct ns_cgroup *ns_cgroup;

	if (!capable(CAP_SYS_ADMIN))
		return ERR_PTR(-EPERM);
	if (!cgroup_is_descendant(cgroup))
		return ERR_PTR(-EPERM);

	ns_cgroup = kzalloc(sizeof(*ns_cgroup), GFP_KERNEL);
	if (!ns_cgroup)
		return ERR_PTR(-ENOMEM);
	return &ns_cgroup->css;
}

static void ns_destroy(struct cgroup_subsys *ss,
			struct cgroup *cgroup)
{
	struct ns_cgroup *ns_cgroup;

	ns_cgroup = cgroup_to_ns(cgroup);
	kfree(ns_cgroup);
}

struct cgroup_subsys ns_subsys = {
	.name = "ns",
	.can_attach = ns_can_attach,
	.create = ns_create,
	.destroy  = ns_destroy,
	.subsys_id = ns_subsys_id,
};
