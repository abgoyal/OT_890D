


#include <linux/ip.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/skbuff.h>
#include <linux/jiffies.h>

/* for sysctl */
#include <linux/fs.h>
#include <linux/sysctl.h>
#include <net/net_namespace.h>

#include <net/ip_vs.h>


#define CHECK_EXPIRE_INTERVAL   (60*HZ)
#define ENTRY_TIMEOUT           (6*60*HZ)

#define COUNT_FOR_FULL_EXPIRATION   30
static int sysctl_ip_vs_lblcr_expiration = 24*60*60*HZ;


#ifndef CONFIG_IP_VS_LBLCR_TAB_BITS
#define CONFIG_IP_VS_LBLCR_TAB_BITS      10
#endif
#define IP_VS_LBLCR_TAB_BITS     CONFIG_IP_VS_LBLCR_TAB_BITS
#define IP_VS_LBLCR_TAB_SIZE     (1 << IP_VS_LBLCR_TAB_BITS)
#define IP_VS_LBLCR_TAB_MASK     (IP_VS_LBLCR_TAB_SIZE - 1)


struct ip_vs_dest_list {
	struct ip_vs_dest_list  *next;          /* list link */
	struct ip_vs_dest       *dest;          /* destination server */
};

struct ip_vs_dest_set {
	atomic_t                size;           /* set size */
	unsigned long           lastmod;        /* last modified time */
	struct ip_vs_dest_list  *list;          /* destination list */
	rwlock_t	        lock;           /* lock for this list */
};


static struct ip_vs_dest_list *
ip_vs_dest_set_insert(struct ip_vs_dest_set *set, struct ip_vs_dest *dest)
{
	struct ip_vs_dest_list *e;

	for (e=set->list; e!=NULL; e=e->next) {
		if (e->dest == dest)
			/* already existed */
			return NULL;
	}

	e = kmalloc(sizeof(*e), GFP_ATOMIC);
	if (e == NULL) {
		IP_VS_ERR("ip_vs_dest_set_insert(): no memory\n");
		return NULL;
	}

	atomic_inc(&dest->refcnt);
	e->dest = dest;

	/* link it to the list */
	e->next = set->list;
	set->list = e;
	atomic_inc(&set->size);

	set->lastmod = jiffies;
	return e;
}

static void
ip_vs_dest_set_erase(struct ip_vs_dest_set *set, struct ip_vs_dest *dest)
{
	struct ip_vs_dest_list *e, **ep;

	for (ep=&set->list, e=*ep; e!=NULL; e=*ep) {
		if (e->dest == dest) {
			/* HIT */
			*ep = e->next;
			atomic_dec(&set->size);
			set->lastmod = jiffies;
			atomic_dec(&e->dest->refcnt);
			kfree(e);
			break;
		}
		ep = &e->next;
	}
}

static void ip_vs_dest_set_eraseall(struct ip_vs_dest_set *set)
{
	struct ip_vs_dest_list *e, **ep;

	write_lock(&set->lock);
	for (ep=&set->list, e=*ep; e!=NULL; e=*ep) {
		*ep = e->next;
		/*
		 * We don't kfree dest because it is refered either
		 * by its service or by the trash dest list.
		 */
		atomic_dec(&e->dest->refcnt);
		kfree(e);
	}
	write_unlock(&set->lock);
}

/* get weighted least-connection node in the destination set */
static inline struct ip_vs_dest *ip_vs_dest_set_min(struct ip_vs_dest_set *set)
{
	register struct ip_vs_dest_list *e;
	struct ip_vs_dest *dest, *least;
	int loh, doh;

	if (set == NULL)
		return NULL;

	/* select the first destination server, whose weight > 0 */
	for (e=set->list; e!=NULL; e=e->next) {
		least = e->dest;
		if (least->flags & IP_VS_DEST_F_OVERLOAD)
			continue;

		if ((atomic_read(&least->weight) > 0)
		    && (least->flags & IP_VS_DEST_F_AVAILABLE)) {
			loh = atomic_read(&least->activeconns) * 50
				+ atomic_read(&least->inactconns);
			goto nextstage;
		}
	}
	return NULL;

	/* find the destination with the weighted least load */
  nextstage:
	for (e=e->next; e!=NULL; e=e->next) {
		dest = e->dest;
		if (dest->flags & IP_VS_DEST_F_OVERLOAD)
			continue;

		doh = atomic_read(&dest->activeconns) * 50
			+ atomic_read(&dest->inactconns);
		if ((loh * atomic_read(&dest->weight) >
		     doh * atomic_read(&least->weight))
		    && (dest->flags & IP_VS_DEST_F_AVAILABLE)) {
			least = dest;
			loh = doh;
		}
	}

	IP_VS_DBG_BUF(6, "ip_vs_dest_set_min: server %s:%d "
		      "activeconns %d refcnt %d weight %d overhead %d\n",
		      IP_VS_DBG_ADDR(least->af, &least->addr),
		      ntohs(least->port),
		      atomic_read(&least->activeconns),
		      atomic_read(&least->refcnt),
		      atomic_read(&least->weight), loh);
	return least;
}


/* get weighted most-connection node in the destination set */
static inline struct ip_vs_dest *ip_vs_dest_set_max(struct ip_vs_dest_set *set)
{
	register struct ip_vs_dest_list *e;
	struct ip_vs_dest *dest, *most;
	int moh, doh;

	if (set == NULL)
		return NULL;

	/* select the first destination server, whose weight > 0 */
	for (e=set->list; e!=NULL; e=e->next) {
		most = e->dest;
		if (atomic_read(&most->weight) > 0) {
			moh = atomic_read(&most->activeconns) * 50
				+ atomic_read(&most->inactconns);
			goto nextstage;
		}
	}
	return NULL;

	/* find the destination with the weighted most load */
  nextstage:
	for (e=e->next; e!=NULL; e=e->next) {
		dest = e->dest;
		doh = atomic_read(&dest->activeconns) * 50
			+ atomic_read(&dest->inactconns);
		/* moh/mw < doh/dw ==> moh*dw < doh*mw, where mw,dw>0 */
		if ((moh * atomic_read(&dest->weight) <
		     doh * atomic_read(&most->weight))
		    && (atomic_read(&dest->weight) > 0)) {
			most = dest;
			moh = doh;
		}
	}

	IP_VS_DBG_BUF(6, "ip_vs_dest_set_max: server %s:%d "
		      "activeconns %d refcnt %d weight %d overhead %d\n",
		      IP_VS_DBG_ADDR(most->af, &most->addr), ntohs(most->port),
		      atomic_read(&most->activeconns),
		      atomic_read(&most->refcnt),
		      atomic_read(&most->weight), moh);
	return most;
}


struct ip_vs_lblcr_entry {
	struct list_head        list;
	int			af;		/* address family */
	union nf_inet_addr      addr;           /* destination IP address */
	struct ip_vs_dest_set   set;            /* destination server set */
	unsigned long           lastuse;        /* last used time */
};


struct ip_vs_lblcr_table {
	struct list_head        bucket[IP_VS_LBLCR_TAB_SIZE];  /* hash bucket */
	atomic_t                entries;        /* number of entries */
	int                     max_size;       /* maximum size of entries */
	struct timer_list       periodic_timer; /* collect stale entries */
	int                     rover;          /* rover for expire check */
	int                     counter;        /* counter for no expire */
};



static ctl_table vs_vars_table[] = {
	{
		.procname	= "lblcr_expiration",
		.data		= &sysctl_ip_vs_lblcr_expiration,
		.maxlen		= sizeof(int),
		.mode		= 0644,
		.proc_handler	= proc_dointvec_jiffies,
	},
	{ .ctl_name = 0 }
};

static struct ctl_table_header * sysctl_header;

static inline void ip_vs_lblcr_free(struct ip_vs_lblcr_entry *en)
{
	list_del(&en->list);
	ip_vs_dest_set_eraseall(&en->set);
	kfree(en);
}


static inline unsigned
ip_vs_lblcr_hashkey(int af, const union nf_inet_addr *addr)
{
	__be32 addr_fold = addr->ip;

#ifdef CONFIG_IP_VS_IPV6
	if (af == AF_INET6)
		addr_fold = addr->ip6[0]^addr->ip6[1]^
			    addr->ip6[2]^addr->ip6[3];
#endif
	return (ntohl(addr_fold)*2654435761UL) & IP_VS_LBLCR_TAB_MASK;
}


static void
ip_vs_lblcr_hash(struct ip_vs_lblcr_table *tbl, struct ip_vs_lblcr_entry *en)
{
	unsigned hash = ip_vs_lblcr_hashkey(en->af, &en->addr);

	list_add(&en->list, &tbl->bucket[hash]);
	atomic_inc(&tbl->entries);
}


static inline struct ip_vs_lblcr_entry *
ip_vs_lblcr_get(int af, struct ip_vs_lblcr_table *tbl,
		const union nf_inet_addr *addr)
{
	unsigned hash = ip_vs_lblcr_hashkey(af, addr);
	struct ip_vs_lblcr_entry *en;

	list_for_each_entry(en, &tbl->bucket[hash], list)
		if (ip_vs_addr_equal(af, &en->addr, addr))
			return en;

	return NULL;
}


static inline struct ip_vs_lblcr_entry *
ip_vs_lblcr_new(struct ip_vs_lblcr_table *tbl, const union nf_inet_addr *daddr,
		struct ip_vs_dest *dest)
{
	struct ip_vs_lblcr_entry *en;

	en = ip_vs_lblcr_get(dest->af, tbl, daddr);
	if (!en) {
		en = kmalloc(sizeof(*en), GFP_ATOMIC);
		if (!en) {
			IP_VS_ERR("ip_vs_lblcr_new(): no memory\n");
			return NULL;
		}

		en->af = dest->af;
		ip_vs_addr_copy(dest->af, &en->addr, daddr);
		en->lastuse = jiffies;

		/* initilize its dest set */
		atomic_set(&(en->set.size), 0);
		en->set.list = NULL;
		rwlock_init(&en->set.lock);

		ip_vs_lblcr_hash(tbl, en);
	}

	write_lock(&en->set.lock);
	ip_vs_dest_set_insert(&en->set, dest);
	write_unlock(&en->set.lock);

	return en;
}


static void ip_vs_lblcr_flush(struct ip_vs_lblcr_table *tbl)
{
	int i;
	struct ip_vs_lblcr_entry *en, *nxt;

	/* No locking required, only called during cleanup. */
	for (i=0; i<IP_VS_LBLCR_TAB_SIZE; i++) {
		list_for_each_entry_safe(en, nxt, &tbl->bucket[i], list) {
			ip_vs_lblcr_free(en);
		}
	}
}


static inline void ip_vs_lblcr_full_check(struct ip_vs_service *svc)
{
	struct ip_vs_lblcr_table *tbl = svc->sched_data;
	unsigned long now = jiffies;
	int i, j;
	struct ip_vs_lblcr_entry *en, *nxt;

	for (i=0, j=tbl->rover; i<IP_VS_LBLCR_TAB_SIZE; i++) {
		j = (j + 1) & IP_VS_LBLCR_TAB_MASK;

		write_lock(&svc->sched_lock);
		list_for_each_entry_safe(en, nxt, &tbl->bucket[j], list) {
			if (time_after(en->lastuse+sysctl_ip_vs_lblcr_expiration,
				       now))
				continue;

			ip_vs_lblcr_free(en);
			atomic_dec(&tbl->entries);
		}
		write_unlock(&svc->sched_lock);
	}
	tbl->rover = j;
}


static void ip_vs_lblcr_check_expire(unsigned long data)
{
	struct ip_vs_service *svc = (struct ip_vs_service *) data;
	struct ip_vs_lblcr_table *tbl = svc->sched_data;
	unsigned long now = jiffies;
	int goal;
	int i, j;
	struct ip_vs_lblcr_entry *en, *nxt;

	if ((tbl->counter % COUNT_FOR_FULL_EXPIRATION) == 0) {
		/* do full expiration check */
		ip_vs_lblcr_full_check(svc);
		tbl->counter = 1;
		goto out;
	}

	if (atomic_read(&tbl->entries) <= tbl->max_size) {
		tbl->counter++;
		goto out;
	}

	goal = (atomic_read(&tbl->entries) - tbl->max_size)*4/3;
	if (goal > tbl->max_size/2)
		goal = tbl->max_size/2;

	for (i=0, j=tbl->rover; i<IP_VS_LBLCR_TAB_SIZE; i++) {
		j = (j + 1) & IP_VS_LBLCR_TAB_MASK;

		write_lock(&svc->sched_lock);
		list_for_each_entry_safe(en, nxt, &tbl->bucket[j], list) {
			if (time_before(now, en->lastuse+ENTRY_TIMEOUT))
				continue;

			ip_vs_lblcr_free(en);
			atomic_dec(&tbl->entries);
			goal--;
		}
		write_unlock(&svc->sched_lock);
		if (goal <= 0)
			break;
	}
	tbl->rover = j;

  out:
	mod_timer(&tbl->periodic_timer, jiffies+CHECK_EXPIRE_INTERVAL);
}

static int ip_vs_lblcr_init_svc(struct ip_vs_service *svc)
{
	int i;
	struct ip_vs_lblcr_table *tbl;

	/*
	 *    Allocate the ip_vs_lblcr_table for this service
	 */
	tbl = kmalloc(sizeof(*tbl), GFP_ATOMIC);
	if (tbl == NULL) {
		IP_VS_ERR("ip_vs_lblcr_init_svc(): no memory\n");
		return -ENOMEM;
	}
	svc->sched_data = tbl;
	IP_VS_DBG(6, "LBLCR hash table (memory=%Zdbytes) allocated for "
		  "current service\n", sizeof(*tbl));

	/*
	 *    Initialize the hash buckets
	 */
	for (i=0; i<IP_VS_LBLCR_TAB_SIZE; i++) {
		INIT_LIST_HEAD(&tbl->bucket[i]);
	}
	tbl->max_size = IP_VS_LBLCR_TAB_SIZE*16;
	tbl->rover = 0;
	tbl->counter = 1;

	/*
	 *    Hook periodic timer for garbage collection
	 */
	setup_timer(&tbl->periodic_timer, ip_vs_lblcr_check_expire,
			(unsigned long)svc);
	mod_timer(&tbl->periodic_timer, jiffies + CHECK_EXPIRE_INTERVAL);

	return 0;
}


static int ip_vs_lblcr_done_svc(struct ip_vs_service *svc)
{
	struct ip_vs_lblcr_table *tbl = svc->sched_data;

	/* remove periodic timer */
	del_timer_sync(&tbl->periodic_timer);

	/* got to clean up table entries here */
	ip_vs_lblcr_flush(tbl);

	/* release the table itself */
	kfree(tbl);
	IP_VS_DBG(6, "LBLCR hash table (memory=%Zdbytes) released\n",
		  sizeof(*tbl));

	return 0;
}


static inline struct ip_vs_dest *
__ip_vs_lblcr_schedule(struct ip_vs_service *svc)
{
	struct ip_vs_dest *dest, *least;
	int loh, doh;

	/*
	 * We think the overhead of processing active connections is fifty
	 * times higher than that of inactive connections in average. (This
	 * fifty times might not be accurate, we will change it later.) We
	 * use the following formula to estimate the overhead:
	 *                dest->activeconns*50 + dest->inactconns
	 * and the load:
	 *                (dest overhead) / dest->weight
	 *
	 * Remember -- no floats in kernel mode!!!
	 * The comparison of h1*w2 > h2*w1 is equivalent to that of
	 *                h1/w1 > h2/w2
	 * if every weight is larger than zero.
	 *
	 * The server with weight=0 is quiesced and will not receive any
	 * new connection.
	 */
	list_for_each_entry(dest, &svc->destinations, n_list) {
		if (dest->flags & IP_VS_DEST_F_OVERLOAD)
			continue;

		if (atomic_read(&dest->weight) > 0) {
			least = dest;
			loh = atomic_read(&least->activeconns) * 50
				+ atomic_read(&least->inactconns);
			goto nextstage;
		}
	}
	return NULL;

	/*
	 *    Find the destination with the least load.
	 */
  nextstage:
	list_for_each_entry_continue(dest, &svc->destinations, n_list) {
		if (dest->flags & IP_VS_DEST_F_OVERLOAD)
			continue;

		doh = atomic_read(&dest->activeconns) * 50
			+ atomic_read(&dest->inactconns);
		if (loh * atomic_read(&dest->weight) >
		    doh * atomic_read(&least->weight)) {
			least = dest;
			loh = doh;
		}
	}

	IP_VS_DBG_BUF(6, "LBLCR: server %s:%d "
		      "activeconns %d refcnt %d weight %d overhead %d\n",
		      IP_VS_DBG_ADDR(least->af, &least->addr),
		      ntohs(least->port),
		      atomic_read(&least->activeconns),
		      atomic_read(&least->refcnt),
		      atomic_read(&least->weight), loh);

	return least;
}


static inline int
is_overloaded(struct ip_vs_dest *dest, struct ip_vs_service *svc)
{
	if (atomic_read(&dest->activeconns) > atomic_read(&dest->weight)) {
		struct ip_vs_dest *d;

		list_for_each_entry(d, &svc->destinations, n_list) {
			if (atomic_read(&d->activeconns)*2
			    < atomic_read(&d->weight)) {
				return 1;
			}
		}
	}
	return 0;
}


static struct ip_vs_dest *
ip_vs_lblcr_schedule(struct ip_vs_service *svc, const struct sk_buff *skb)
{
	struct ip_vs_lblcr_table *tbl = svc->sched_data;
	struct ip_vs_iphdr iph;
	struct ip_vs_dest *dest = NULL;
	struct ip_vs_lblcr_entry *en;

	ip_vs_fill_iphdr(svc->af, skb_network_header(skb), &iph);

	IP_VS_DBG(6, "ip_vs_lblcr_schedule(): Scheduling...\n");

	/* First look in our cache */
	read_lock(&svc->sched_lock);
	en = ip_vs_lblcr_get(svc->af, tbl, &iph.daddr);
	if (en) {
		/* We only hold a read lock, but this is atomic */
		en->lastuse = jiffies;

		/* Get the least loaded destination */
		read_lock(&en->set.lock);
		dest = ip_vs_dest_set_min(&en->set);
		read_unlock(&en->set.lock);

		/* More than one destination + enough time passed by, cleanup */
		if (atomic_read(&en->set.size) > 1 &&
				time_after(jiffies, en->set.lastmod +
				sysctl_ip_vs_lblcr_expiration)) {
			struct ip_vs_dest *m;

			write_lock(&en->set.lock);
			m = ip_vs_dest_set_max(&en->set);
			if (m)
				ip_vs_dest_set_erase(&en->set, m);
			write_unlock(&en->set.lock);
		}

		/* If the destination is not overloaded, use it */
		if (dest && !is_overloaded(dest, svc)) {
			read_unlock(&svc->sched_lock);
			goto out;
		}

		/* The cache entry is invalid, time to schedule */
		dest = __ip_vs_lblcr_schedule(svc);
		if (!dest) {
			IP_VS_ERR_RL("LBLCR: no destination available\n");
			read_unlock(&svc->sched_lock);
			return NULL;
		}

		/* Update our cache entry */
		write_lock(&en->set.lock);
		ip_vs_dest_set_insert(&en->set, dest);
		write_unlock(&en->set.lock);
	}
	read_unlock(&svc->sched_lock);

	if (dest)
		goto out;

	/* No cache entry, time to schedule */
	dest = __ip_vs_lblcr_schedule(svc);
	if (!dest) {
		IP_VS_DBG(1, "no destination available\n");
		return NULL;
	}

	/* If we fail to create a cache entry, we'll just use the valid dest */
	write_lock(&svc->sched_lock);
	ip_vs_lblcr_new(tbl, &iph.daddr, dest);
	write_unlock(&svc->sched_lock);

out:
	IP_VS_DBG_BUF(6, "LBLCR: destination IP address %s --> server %s:%d\n",
		      IP_VS_DBG_ADDR(svc->af, &iph.daddr),
		      IP_VS_DBG_ADDR(svc->af, &dest->addr), ntohs(dest->port));

	return dest;
}


static struct ip_vs_scheduler ip_vs_lblcr_scheduler =
{
	.name =			"lblcr",
	.refcnt =		ATOMIC_INIT(0),
	.module =		THIS_MODULE,
	.n_list =		LIST_HEAD_INIT(ip_vs_lblcr_scheduler.n_list),
	.init_service =		ip_vs_lblcr_init_svc,
	.done_service =		ip_vs_lblcr_done_svc,
	.schedule =		ip_vs_lblcr_schedule,
};


static int __init ip_vs_lblcr_init(void)
{
	int ret;

	sysctl_header = register_sysctl_paths(net_vs_ctl_path, vs_vars_table);
	ret = register_ip_vs_scheduler(&ip_vs_lblcr_scheduler);
	if (ret)
		unregister_sysctl_table(sysctl_header);
	return ret;
}


static void __exit ip_vs_lblcr_cleanup(void)
{
	unregister_sysctl_table(sysctl_header);
	unregister_ip_vs_scheduler(&ip_vs_lblcr_scheduler);
}


module_init(ip_vs_lblcr_init);
module_exit(ip_vs_lblcr_cleanup);
MODULE_LICENSE("GPL");
