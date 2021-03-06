

#include <linux/module.h>
#include <linux/spinlock.h>
#include <linux/interrupt.h>
#include <asm/string.h>
#include <linux/kmod.h>
#include <linux/sysctl.h>

#include <net/ip_vs.h>

static LIST_HEAD(ip_vs_schedulers);

/* lock for service table */
static DEFINE_RWLOCK(__ip_vs_sched_lock);


int ip_vs_bind_scheduler(struct ip_vs_service *svc,
			 struct ip_vs_scheduler *scheduler)
{
	int ret;

	if (svc == NULL) {
		IP_VS_ERR("ip_vs_bind_scheduler(): svc arg NULL\n");
		return -EINVAL;
	}
	if (scheduler == NULL) {
		IP_VS_ERR("ip_vs_bind_scheduler(): scheduler arg NULL\n");
		return -EINVAL;
	}

	svc->scheduler = scheduler;

	if (scheduler->init_service) {
		ret = scheduler->init_service(svc);
		if (ret) {
			IP_VS_ERR("ip_vs_bind_scheduler(): init error\n");
			return ret;
		}
	}

	return 0;
}


int ip_vs_unbind_scheduler(struct ip_vs_service *svc)
{
	struct ip_vs_scheduler *sched;

	if (svc == NULL) {
		IP_VS_ERR("ip_vs_unbind_scheduler(): svc arg NULL\n");
		return -EINVAL;
	}

	sched = svc->scheduler;
	if (sched == NULL) {
		IP_VS_ERR("ip_vs_unbind_scheduler(): svc isn't bound\n");
		return -EINVAL;
	}

	if (sched->done_service) {
		if (sched->done_service(svc) != 0) {
			IP_VS_ERR("ip_vs_unbind_scheduler(): done error\n");
			return -EINVAL;
		}
	}

	svc->scheduler = NULL;
	return 0;
}


static struct ip_vs_scheduler *ip_vs_sched_getbyname(const char *sched_name)
{
	struct ip_vs_scheduler *sched;

	IP_VS_DBG(2, "ip_vs_sched_getbyname(): sched_name \"%s\"\n",
		  sched_name);

	read_lock_bh(&__ip_vs_sched_lock);

	list_for_each_entry(sched, &ip_vs_schedulers, n_list) {
		/*
		 * Test and get the modules atomically
		 */
		if (sched->module && !try_module_get(sched->module)) {
			/*
			 * This scheduler is just deleted
			 */
			continue;
		}
		if (strcmp(sched_name, sched->name)==0) {
			/* HIT */
			read_unlock_bh(&__ip_vs_sched_lock);
			return sched;
		}
		if (sched->module)
			module_put(sched->module);
	}

	read_unlock_bh(&__ip_vs_sched_lock);
	return NULL;
}


struct ip_vs_scheduler *ip_vs_scheduler_get(const char *sched_name)
{
	struct ip_vs_scheduler *sched;

	/*
	 *  Search for the scheduler by sched_name
	 */
	sched = ip_vs_sched_getbyname(sched_name);

	/*
	 *  If scheduler not found, load the module and search again
	 */
	if (sched == NULL) {
		request_module("ip_vs_%s", sched_name);
		sched = ip_vs_sched_getbyname(sched_name);
	}

	return sched;
}

void ip_vs_scheduler_put(struct ip_vs_scheduler *scheduler)
{
	if (scheduler->module)
		module_put(scheduler->module);
}


int register_ip_vs_scheduler(struct ip_vs_scheduler *scheduler)
{
	struct ip_vs_scheduler *sched;

	if (!scheduler) {
		IP_VS_ERR("register_ip_vs_scheduler(): NULL arg\n");
		return -EINVAL;
	}

	if (!scheduler->name) {
		IP_VS_ERR("register_ip_vs_scheduler(): NULL scheduler_name\n");
		return -EINVAL;
	}

	/* increase the module use count */
	ip_vs_use_count_inc();

	write_lock_bh(&__ip_vs_sched_lock);

	if (!list_empty(&scheduler->n_list)) {
		write_unlock_bh(&__ip_vs_sched_lock);
		ip_vs_use_count_dec();
		IP_VS_ERR("register_ip_vs_scheduler(): [%s] scheduler "
			  "already linked\n", scheduler->name);
		return -EINVAL;
	}

	/*
	 *  Make sure that the scheduler with this name doesn't exist
	 *  in the scheduler list.
	 */
	list_for_each_entry(sched, &ip_vs_schedulers, n_list) {
		if (strcmp(scheduler->name, sched->name) == 0) {
			write_unlock_bh(&__ip_vs_sched_lock);
			ip_vs_use_count_dec();
			IP_VS_ERR("register_ip_vs_scheduler(): [%s] scheduler "
					"already existed in the system\n",
					scheduler->name);
			return -EINVAL;
		}
	}
	/*
	 *	Add it into the d-linked scheduler list
	 */
	list_add(&scheduler->n_list, &ip_vs_schedulers);
	write_unlock_bh(&__ip_vs_sched_lock);

	IP_VS_INFO("[%s] scheduler registered.\n", scheduler->name);

	return 0;
}


int unregister_ip_vs_scheduler(struct ip_vs_scheduler *scheduler)
{
	if (!scheduler) {
		IP_VS_ERR( "unregister_ip_vs_scheduler(): NULL arg\n");
		return -EINVAL;
	}

	write_lock_bh(&__ip_vs_sched_lock);
	if (list_empty(&scheduler->n_list)) {
		write_unlock_bh(&__ip_vs_sched_lock);
		IP_VS_ERR("unregister_ip_vs_scheduler(): [%s] scheduler "
			  "is not in the list. failed\n", scheduler->name);
		return -EINVAL;
	}

	/*
	 *	Remove it from the d-linked scheduler list
	 */
	list_del(&scheduler->n_list);
	write_unlock_bh(&__ip_vs_sched_lock);

	/* decrease the module use count */
	ip_vs_use_count_dec();

	IP_VS_INFO("[%s] scheduler unregistered.\n", scheduler->name);

	return 0;
}
