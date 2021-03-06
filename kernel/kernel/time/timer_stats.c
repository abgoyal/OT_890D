

#include <linux/proc_fs.h>
#include <linux/module.h>
#include <linux/spinlock.h>
#include <linux/sched.h>
#include <linux/seq_file.h>
#include <linux/kallsyms.h>

#include <asm/uaccess.h>

struct entry {
	/*
	 * Hash list:
	 */
	struct entry		*next;

	/*
	 * Hash keys:
	 */
	void			*timer;
	void			*start_func;
	void			*expire_func;
	pid_t			pid;

	/*
	 * Number of timeout events:
	 */
	unsigned long		count;
	unsigned int		timer_flag;

	/*
	 * We save the command-line string to preserve
	 * this information past task exit:
	 */
	char			comm[TASK_COMM_LEN + 1];

} ____cacheline_aligned_in_smp;

static DEFINE_SPINLOCK(table_lock);

static DEFINE_PER_CPU(spinlock_t, lookup_lock);

static DEFINE_MUTEX(show_mutex);

static int __read_mostly active;

static ktime_t time_start, time_stop;

#define MAX_ENTRIES_BITS	10
#define MAX_ENTRIES		(1UL << MAX_ENTRIES_BITS)

static unsigned long nr_entries;
static struct entry entries[MAX_ENTRIES];

static atomic_t overflow_count;

#define TSTAT_HASH_BITS		(MAX_ENTRIES_BITS - 1)
#define TSTAT_HASH_SIZE		(1UL << TSTAT_HASH_BITS)
#define TSTAT_HASH_MASK		(TSTAT_HASH_SIZE - 1)

#define __tstat_hashfn(entry)						\
	(((unsigned long)(entry)->timer       ^				\
	  (unsigned long)(entry)->start_func  ^				\
	  (unsigned long)(entry)->expire_func ^				\
	  (unsigned long)(entry)->pid		) & TSTAT_HASH_MASK)

#define tstat_hashentry(entry)	(tstat_hash_table + __tstat_hashfn(entry))

static struct entry *tstat_hash_table[TSTAT_HASH_SIZE] __read_mostly;

static void reset_entries(void)
{
	nr_entries = 0;
	memset(entries, 0, sizeof(entries));
	memset(tstat_hash_table, 0, sizeof(tstat_hash_table));
	atomic_set(&overflow_count, 0);
}

static struct entry *alloc_entry(void)
{
	if (nr_entries >= MAX_ENTRIES)
		return NULL;

	return entries + nr_entries++;
}

static int match_entries(struct entry *entry1, struct entry *entry2)
{
	return entry1->timer       == entry2->timer	  &&
	       entry1->start_func  == entry2->start_func  &&
	       entry1->expire_func == entry2->expire_func &&
	       entry1->pid	   == entry2->pid;
}

static struct entry *tstat_lookup(struct entry *entry, char *comm)
{
	struct entry **head, *curr, *prev;

	head = tstat_hashentry(entry);
	curr = *head;

	/*
	 * The fastpath is when the entry is already hashed,
	 * we do this with the lookup lock held, but with the
	 * table lock not held:
	 */
	while (curr) {
		if (match_entries(curr, entry))
			return curr;

		curr = curr->next;
	}
	/*
	 * Slowpath: allocate, set up and link a new hash entry:
	 */
	prev = NULL;
	curr = *head;

	spin_lock(&table_lock);
	/*
	 * Make sure we have not raced with another CPU:
	 */
	while (curr) {
		if (match_entries(curr, entry))
			goto out_unlock;

		prev = curr;
		curr = curr->next;
	}

	curr = alloc_entry();
	if (curr) {
		*curr = *entry;
		curr->count = 0;
		curr->next = NULL;
		memcpy(curr->comm, comm, TASK_COMM_LEN);

		smp_mb(); /* Ensure that curr is initialized before insert */

		if (prev)
			prev->next = curr;
		else
			*head = curr;
	}
 out_unlock:
	spin_unlock(&table_lock);

	return curr;
}

void timer_stats_update_stats(void *timer, pid_t pid, void *startf,
			      void *timerf, char *comm,
			      unsigned int timer_flag)
{
	/*
	 * It doesnt matter which lock we take:
	 */
	spinlock_t *lock;
	struct entry *entry, input;
	unsigned long flags;

	if (likely(!active))
		return;

	lock = &per_cpu(lookup_lock, raw_smp_processor_id());

	input.timer = timer;
	input.start_func = startf;
	input.expire_func = timerf;
	input.pid = pid;
	input.timer_flag = timer_flag;

	spin_lock_irqsave(lock, flags);
	if (!active)
		goto out_unlock;

	entry = tstat_lookup(&input, comm);
	if (likely(entry))
		entry->count++;
	else
		atomic_inc(&overflow_count);

 out_unlock:
	spin_unlock_irqrestore(lock, flags);
}

static void print_name_offset(struct seq_file *m, unsigned long addr)
{
	char symname[KSYM_NAME_LEN];

	if (lookup_symbol_name(addr, symname) < 0)
		seq_printf(m, "<%p>", (void *)addr);
	else
		seq_printf(m, "%s", symname);
}

static int tstats_show(struct seq_file *m, void *v)
{
	struct timespec period;
	struct entry *entry;
	unsigned long ms;
	long events = 0;
	ktime_t time;
	int i;

	mutex_lock(&show_mutex);
	/*
	 * If still active then calculate up to now:
	 */
	if (active)
		time_stop = ktime_get();

	time = ktime_sub(time_stop, time_start);

	period = ktime_to_timespec(time);
	ms = period.tv_nsec / 1000000;

	seq_puts(m, "Timer Stats Version: v0.2\n");
	seq_printf(m, "Sample period: %ld.%03ld s\n", period.tv_sec, ms);
	if (atomic_read(&overflow_count))
		seq_printf(m, "Overflow: %d entries\n",
			atomic_read(&overflow_count));

	for (i = 0; i < nr_entries; i++) {
		entry = entries + i;
 		if (entry->timer_flag & TIMER_STATS_FLAG_DEFERRABLE) {
			seq_printf(m, "%4luD, %5d %-16s ",
				entry->count, entry->pid, entry->comm);
		} else {
			seq_printf(m, " %4lu, %5d %-16s ",
				entry->count, entry->pid, entry->comm);
		}

		print_name_offset(m, (unsigned long)entry->start_func);
		seq_puts(m, " (");
		print_name_offset(m, (unsigned long)entry->expire_func);
		seq_puts(m, ")\n");

		events += entry->count;
	}

	ms += period.tv_sec * 1000;
	if (!ms)
		ms = 1;

	if (events && period.tv_sec)
		seq_printf(m, "%ld total events, %ld.%03ld events/sec\n",
			   events, events * 1000 / ms,
			   (events * 1000000 / ms) % 1000);
	else
		seq_printf(m, "%ld total events\n", events);

	mutex_unlock(&show_mutex);

	return 0;
}

static void sync_access(void)
{
	unsigned long flags;
	int cpu;

	for_each_online_cpu(cpu) {
		spin_lock_irqsave(&per_cpu(lookup_lock, cpu), flags);
		/* nothing */
		spin_unlock_irqrestore(&per_cpu(lookup_lock, cpu), flags);
	}
}

static ssize_t tstats_write(struct file *file, const char __user *buf,
			    size_t count, loff_t *offs)
{
	char ctl[2];

	if (count != 2 || *offs)
		return -EINVAL;

	if (copy_from_user(ctl, buf, count))
		return -EFAULT;

	mutex_lock(&show_mutex);
	switch (ctl[0]) {
	case '0':
		if (active) {
			active = 0;
			time_stop = ktime_get();
			sync_access();
		}
		break;
	case '1':
		if (!active) {
			reset_entries();
			time_start = ktime_get();
			smp_mb();
			active = 1;
		}
		break;
	default:
		count = -EINVAL;
	}
	mutex_unlock(&show_mutex);

	return count;
}

static int tstats_open(struct inode *inode, struct file *filp)
{
	return single_open(filp, tstats_show, NULL);
}

static struct file_operations tstats_fops = {
	.open		= tstats_open,
	.read		= seq_read,
	.write		= tstats_write,
	.llseek		= seq_lseek,
	.release	= single_release,
};

void __init init_timer_stats(void)
{
	int cpu;

	for_each_possible_cpu(cpu)
		spin_lock_init(&per_cpu(lookup_lock, cpu));
}

static int __init init_tstats_procfs(void)
{
	struct proc_dir_entry *pe;

	pe = proc_create("timer_stats", 0644, NULL, &tstats_fops);
	if (!pe)
		return -ENOMEM;
	return 0;
}
__initcall(init_tstats_procfs);
