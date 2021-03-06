

#include <linux/module.h>
#include <linux/time.h>
#include <linux/fs.h>
#include <linux/jbd2.h>
#include <linux/errno.h>
#include <linux/slab.h>
#include <linux/init.h>
#include <linux/mm.h>
#include <linux/freezer.h>
#include <linux/pagemap.h>
#include <linux/kthread.h>
#include <linux/poison.h>
#include <linux/proc_fs.h>
#include <linux/debugfs.h>
#include <linux/seq_file.h>
#include <linux/math64.h>

#include <asm/uaccess.h>
#include <asm/page.h>

EXPORT_SYMBOL(jbd2_journal_start);
EXPORT_SYMBOL(jbd2_journal_restart);
EXPORT_SYMBOL(jbd2_journal_extend);
EXPORT_SYMBOL(jbd2_journal_stop);
EXPORT_SYMBOL(jbd2_journal_lock_updates);
EXPORT_SYMBOL(jbd2_journal_unlock_updates);
EXPORT_SYMBOL(jbd2_journal_get_write_access);
EXPORT_SYMBOL(jbd2_journal_get_create_access);
EXPORT_SYMBOL(jbd2_journal_get_undo_access);
EXPORT_SYMBOL(jbd2_journal_set_triggers);
EXPORT_SYMBOL(jbd2_journal_dirty_metadata);
EXPORT_SYMBOL(jbd2_journal_release_buffer);
EXPORT_SYMBOL(jbd2_journal_forget);
#if 0
EXPORT_SYMBOL(journal_sync_buffer);
#endif
EXPORT_SYMBOL(jbd2_journal_flush);
EXPORT_SYMBOL(jbd2_journal_revoke);

EXPORT_SYMBOL(jbd2_journal_init_dev);
EXPORT_SYMBOL(jbd2_journal_init_inode);
EXPORT_SYMBOL(jbd2_journal_update_format);
EXPORT_SYMBOL(jbd2_journal_check_used_features);
EXPORT_SYMBOL(jbd2_journal_check_available_features);
EXPORT_SYMBOL(jbd2_journal_set_features);
EXPORT_SYMBOL(jbd2_journal_load);
EXPORT_SYMBOL(jbd2_journal_destroy);
EXPORT_SYMBOL(jbd2_journal_abort);
EXPORT_SYMBOL(jbd2_journal_errno);
EXPORT_SYMBOL(jbd2_journal_ack_err);
EXPORT_SYMBOL(jbd2_journal_clear_err);
EXPORT_SYMBOL(jbd2_log_wait_commit);
EXPORT_SYMBOL(jbd2_journal_start_commit);
EXPORT_SYMBOL(jbd2_journal_force_commit_nested);
EXPORT_SYMBOL(jbd2_journal_wipe);
EXPORT_SYMBOL(jbd2_journal_blocks_per_page);
EXPORT_SYMBOL(jbd2_journal_invalidatepage);
EXPORT_SYMBOL(jbd2_journal_try_to_free_buffers);
EXPORT_SYMBOL(jbd2_journal_force_commit);
EXPORT_SYMBOL(jbd2_journal_file_inode);
EXPORT_SYMBOL(jbd2_journal_init_jbd_inode);
EXPORT_SYMBOL(jbd2_journal_release_jbd_inode);
EXPORT_SYMBOL(jbd2_journal_begin_ordered_truncate);

static int journal_convert_superblock_v1(journal_t *, journal_superblock_t *);
static void __journal_abort_soft (journal_t *journal, int errno);


static void commit_timeout(unsigned long __data)
{
	struct task_struct * p = (struct task_struct *) __data;

	wake_up_process(p);
}


static int kjournald2(void *arg)
{
	journal_t *journal = arg;
	transaction_t *transaction;

	/*
	 * Set up an interval timer which can be used to trigger a commit wakeup
	 * after the commit interval expires
	 */
	setup_timer(&journal->j_commit_timer, commit_timeout,
			(unsigned long)current);

	/* Record that the journal thread is running */
	journal->j_task = current;
	wake_up(&journal->j_wait_done_commit);

	printk(KERN_INFO "kjournald2 starting: pid %d, dev %s, "
	       "commit interval %ld seconds\n", current->pid,
	       journal->j_devname, journal->j_commit_interval / HZ);

	/*
	 * And now, wait forever for commit wakeup events.
	 */
	spin_lock(&journal->j_state_lock);

loop:
	if (journal->j_flags & JBD2_UNMOUNT)
		goto end_loop;

	jbd_debug(1, "commit_sequence=%d, commit_request=%d\n",
		journal->j_commit_sequence, journal->j_commit_request);

	if (journal->j_commit_sequence != journal->j_commit_request) {
		jbd_debug(1, "OK, requests differ\n");
		spin_unlock(&journal->j_state_lock);
		del_timer_sync(&journal->j_commit_timer);
		jbd2_journal_commit_transaction(journal);
		spin_lock(&journal->j_state_lock);
		goto loop;
	}

	wake_up(&journal->j_wait_done_commit);
	if (freezing(current)) {
		/*
		 * The simpler the better. Flushing journal isn't a
		 * good idea, because that depends on threads that may
		 * be already stopped.
		 */
		jbd_debug(1, "Now suspending kjournald2\n");
		spin_unlock(&journal->j_state_lock);
		refrigerator();
		spin_lock(&journal->j_state_lock);
	} else {
		/*
		 * We assume on resume that commits are already there,
		 * so we don't sleep
		 */
		DEFINE_WAIT(wait);
		int should_sleep = 1;

		prepare_to_wait(&journal->j_wait_commit, &wait,
				TASK_INTERRUPTIBLE);
		if (journal->j_commit_sequence != journal->j_commit_request)
			should_sleep = 0;
		transaction = journal->j_running_transaction;
		if (transaction && time_after_eq(jiffies,
						transaction->t_expires))
			should_sleep = 0;
		if (journal->j_flags & JBD2_UNMOUNT)
			should_sleep = 0;
		if (should_sleep) {
			spin_unlock(&journal->j_state_lock);
			schedule();
			spin_lock(&journal->j_state_lock);
		}
		finish_wait(&journal->j_wait_commit, &wait);
	}

	jbd_debug(1, "kjournald2 wakes\n");

	/*
	 * Were we woken up by a commit wakeup event?
	 */
	transaction = journal->j_running_transaction;
	if (transaction && time_after_eq(jiffies, transaction->t_expires)) {
		journal->j_commit_request = transaction->t_tid;
		jbd_debug(1, "woke because of timeout\n");
	}
	goto loop;

end_loop:
	spin_unlock(&journal->j_state_lock);
	del_timer_sync(&journal->j_commit_timer);
	journal->j_task = NULL;
	wake_up(&journal->j_wait_done_commit);
	jbd_debug(1, "Journal thread exiting.\n");
	return 0;
}

static int jbd2_journal_start_thread(journal_t *journal)
{
	struct task_struct *t;

	t = kthread_run(kjournald2, journal, "kjournald2");
	if (IS_ERR(t))
		return PTR_ERR(t);

	wait_event(journal->j_wait_done_commit, journal->j_task != NULL);
	return 0;
}

static void journal_kill_thread(journal_t *journal)
{
	spin_lock(&journal->j_state_lock);
	journal->j_flags |= JBD2_UNMOUNT;

	while (journal->j_task) {
		wake_up(&journal->j_wait_commit);
		spin_unlock(&journal->j_state_lock);
		wait_event(journal->j_wait_done_commit, journal->j_task == NULL);
		spin_lock(&journal->j_state_lock);
	}
	spin_unlock(&journal->j_state_lock);
}


int jbd2_journal_write_metadata_buffer(transaction_t *transaction,
				  struct journal_head  *jh_in,
				  struct journal_head **jh_out,
				  unsigned long long blocknr)
{
	int need_copy_out = 0;
	int done_copy_out = 0;
	int do_escape = 0;
	char *mapped_data;
	struct buffer_head *new_bh;
	struct journal_head *new_jh;
	struct page *new_page;
	unsigned int new_offset;
	struct buffer_head *bh_in = jh2bh(jh_in);
	struct jbd2_buffer_trigger_type *triggers;

	/*
	 * The buffer really shouldn't be locked: only the current committing
	 * transaction is allowed to write it, so nobody else is allowed
	 * to do any IO.
	 *
	 * akpm: except if we're journalling data, and write() output is
	 * also part of a shared mapping, and another thread has
	 * decided to launch a writepage() against this buffer.
	 */
	J_ASSERT_BH(bh_in, buffer_jbddirty(bh_in));

	new_bh = alloc_buffer_head(GFP_NOFS|__GFP_NOFAIL);

	/*
	 * If a new transaction has already done a buffer copy-out, then
	 * we use that version of the data for the commit.
	 */
	jbd_lock_bh_state(bh_in);
repeat:
	if (jh_in->b_frozen_data) {
		done_copy_out = 1;
		new_page = virt_to_page(jh_in->b_frozen_data);
		new_offset = offset_in_page(jh_in->b_frozen_data);
		triggers = jh_in->b_frozen_triggers;
	} else {
		new_page = jh2bh(jh_in)->b_page;
		new_offset = offset_in_page(jh2bh(jh_in)->b_data);
		triggers = jh_in->b_triggers;
	}

	mapped_data = kmap_atomic(new_page, KM_USER0);
	/*
	 * Fire any commit trigger.  Do this before checking for escaping,
	 * as the trigger may modify the magic offset.  If a copy-out
	 * happens afterwards, it will have the correct data in the buffer.
	 */
	jbd2_buffer_commit_trigger(jh_in, mapped_data + new_offset,
				   triggers);

	/*
	 * Check for escaping
	 */
	if (*((__be32 *)(mapped_data + new_offset)) ==
				cpu_to_be32(JBD2_MAGIC_NUMBER)) {
		need_copy_out = 1;
		do_escape = 1;
	}
	kunmap_atomic(mapped_data, KM_USER0);

	/*
	 * Do we need to do a data copy?
	 */
	if (need_copy_out && !done_copy_out) {
		char *tmp;

		jbd_unlock_bh_state(bh_in);
		tmp = jbd2_alloc(bh_in->b_size, GFP_NOFS);
		jbd_lock_bh_state(bh_in);
		if (jh_in->b_frozen_data) {
			jbd2_free(tmp, bh_in->b_size);
			goto repeat;
		}

		jh_in->b_frozen_data = tmp;
		mapped_data = kmap_atomic(new_page, KM_USER0);
		memcpy(tmp, mapped_data + new_offset, jh2bh(jh_in)->b_size);
		kunmap_atomic(mapped_data, KM_USER0);

		new_page = virt_to_page(tmp);
		new_offset = offset_in_page(tmp);
		done_copy_out = 1;

		/*
		 * This isn't strictly necessary, as we're using frozen
		 * data for the escaping, but it keeps consistency with
		 * b_frozen_data usage.
		 */
		jh_in->b_frozen_triggers = jh_in->b_triggers;
	}

	/*
	 * Did we need to do an escaping?  Now we've done all the
	 * copying, we can finally do so.
	 */
	if (do_escape) {
		mapped_data = kmap_atomic(new_page, KM_USER0);
		*((unsigned int *)(mapped_data + new_offset)) = 0;
		kunmap_atomic(mapped_data, KM_USER0);
	}

	/* keep subsequent assertions sane */
	new_bh->b_state = 0;
	init_buffer(new_bh, NULL, NULL);
	atomic_set(&new_bh->b_count, 1);
	jbd_unlock_bh_state(bh_in);

	new_jh = jbd2_journal_add_journal_head(new_bh);	/* This sleeps */

	set_bh_page(new_bh, new_page, new_offset);
	new_jh->b_transaction = NULL;
	new_bh->b_size = jh2bh(jh_in)->b_size;
	new_bh->b_bdev = transaction->t_journal->j_dev;
	new_bh->b_blocknr = blocknr;
	set_buffer_mapped(new_bh);
	set_buffer_dirty(new_bh);

	*jh_out = new_jh;

	/*
	 * The to-be-written buffer needs to get moved to the io queue,
	 * and the original buffer whose contents we are shadowing or
	 * copying is moved to the transaction's shadow queue.
	 */
	JBUFFER_TRACE(jh_in, "file as BJ_Shadow");
	jbd2_journal_file_buffer(jh_in, transaction, BJ_Shadow);
	JBUFFER_TRACE(new_jh, "file as BJ_IO");
	jbd2_journal_file_buffer(new_jh, transaction, BJ_IO);

	return do_escape | (done_copy_out << 1);
}



int __jbd2_log_space_left(journal_t *journal)
{
	int left = journal->j_free;

	assert_spin_locked(&journal->j_state_lock);

	/*
	 * Be pessimistic here about the number of those free blocks which
	 * might be required for log descriptor control blocks.
	 */

#define MIN_LOG_RESERVED_BLOCKS 32 /* Allow for rounding errors */

	left -= MIN_LOG_RESERVED_BLOCKS;

	if (left <= 0)
		return 0;
	left -= (left >> 3);
	return left;
}

int __jbd2_log_start_commit(journal_t *journal, tid_t target)
{
	/*
	 * Are we already doing a recent enough commit?
	 */
	if (!tid_geq(journal->j_commit_request, target)) {
		/*
		 * We want a new commit: OK, mark the request and wakup the
		 * commit thread.  We do _not_ do the commit ourselves.
		 */

		journal->j_commit_request = target;
		jbd_debug(1, "JBD: requesting commit %d/%d\n",
			  journal->j_commit_request,
			  journal->j_commit_sequence);
		wake_up(&journal->j_wait_commit);
		return 1;
	}
	return 0;
}

int jbd2_log_start_commit(journal_t *journal, tid_t tid)
{
	int ret;

	spin_lock(&journal->j_state_lock);
	ret = __jbd2_log_start_commit(journal, tid);
	spin_unlock(&journal->j_state_lock);
	return ret;
}

int jbd2_journal_force_commit_nested(journal_t *journal)
{
	transaction_t *transaction = NULL;
	tid_t tid;

	spin_lock(&journal->j_state_lock);
	if (journal->j_running_transaction && !current->journal_info) {
		transaction = journal->j_running_transaction;
		__jbd2_log_start_commit(journal, transaction->t_tid);
	} else if (journal->j_committing_transaction)
		transaction = journal->j_committing_transaction;

	if (!transaction) {
		spin_unlock(&journal->j_state_lock);
		return 0;	/* Nothing to retry */
	}

	tid = transaction->t_tid;
	spin_unlock(&journal->j_state_lock);
	jbd2_log_wait_commit(journal, tid);
	return 1;
}

int jbd2_journal_start_commit(journal_t *journal, tid_t *ptid)
{
	int ret = 0;

	spin_lock(&journal->j_state_lock);
	if (journal->j_running_transaction) {
		tid_t tid = journal->j_running_transaction->t_tid;

		__jbd2_log_start_commit(journal, tid);
		/* There's a running transaction and we've just made sure
		 * it's commit has been scheduled. */
		if (ptid)
			*ptid = tid;
		ret = 1;
	} else if (journal->j_committing_transaction) {
		/*
		 * If ext3_write_super() recently started a commit, then we
		 * have to wait for completion of that transaction
		 */
		if (ptid)
			*ptid = journal->j_committing_transaction->t_tid;
		ret = 1;
	}
	spin_unlock(&journal->j_state_lock);
	return ret;
}

int jbd2_log_wait_commit(journal_t *journal, tid_t tid)
{
	int err = 0;

#ifdef CONFIG_JBD2_DEBUG
	spin_lock(&journal->j_state_lock);
	if (!tid_geq(journal->j_commit_request, tid)) {
		printk(KERN_EMERG
		       "%s: error: j_commit_request=%d, tid=%d\n",
		       __func__, journal->j_commit_request, tid);
	}
	spin_unlock(&journal->j_state_lock);
#endif
	spin_lock(&journal->j_state_lock);
	while (tid_gt(tid, journal->j_commit_sequence)) {
		jbd_debug(1, "JBD: want %d, j_commit_sequence=%d\n",
				  tid, journal->j_commit_sequence);
		wake_up(&journal->j_wait_commit);
		spin_unlock(&journal->j_state_lock);
		wait_event(journal->j_wait_done_commit,
				!tid_gt(tid, journal->j_commit_sequence));
		spin_lock(&journal->j_state_lock);
	}
	spin_unlock(&journal->j_state_lock);

	if (unlikely(is_journal_aborted(journal))) {
		printk(KERN_EMERG "journal commit I/O error\n");
		err = -EIO;
	}
	return err;
}


int jbd2_journal_next_log_block(journal_t *journal, unsigned long long *retp)
{
	unsigned long blocknr;

	spin_lock(&journal->j_state_lock);
	J_ASSERT(journal->j_free > 1);

	blocknr = journal->j_head;
	journal->j_head++;
	journal->j_free--;
	if (journal->j_head == journal->j_last)
		journal->j_head = journal->j_first;
	spin_unlock(&journal->j_state_lock);
	return jbd2_journal_bmap(journal, blocknr, retp);
}

int jbd2_journal_bmap(journal_t *journal, unsigned long blocknr,
		 unsigned long long *retp)
{
	int err = 0;
	unsigned long long ret;

	if (journal->j_inode) {
		ret = bmap(journal->j_inode, blocknr);
		if (ret)
			*retp = ret;
		else {
			printk(KERN_ALERT "%s: journal block not found "
					"at offset %lu on %s\n",
			       __func__, blocknr, journal->j_devname);
			err = -EIO;
			__journal_abort_soft(journal, err);
		}
	} else {
		*retp = blocknr; /* +journal->j_blk_offset */
	}
	return err;
}

struct journal_head *jbd2_journal_get_descriptor_buffer(journal_t *journal)
{
	struct buffer_head *bh;
	unsigned long long blocknr;
	int err;

	err = jbd2_journal_next_log_block(journal, &blocknr);

	if (err)
		return NULL;

	bh = __getblk(journal->j_dev, blocknr, journal->j_blocksize);
	if (!bh)
		return NULL;
	lock_buffer(bh);
	memset(bh->b_data, 0, journal->j_blocksize);
	set_buffer_uptodate(bh);
	unlock_buffer(bh);
	BUFFER_TRACE(bh, "return this buffer");
	return jbd2_journal_add_journal_head(bh);
}

struct jbd2_stats_proc_session {
	journal_t *journal;
	struct transaction_stats_s *stats;
	int start;
	int max;
};

static void *jbd2_history_skip_empty(struct jbd2_stats_proc_session *s,
					struct transaction_stats_s *ts,
					int first)
{
	if (ts == s->stats + s->max)
		ts = s->stats;
	if (!first && ts == s->stats + s->start)
		return NULL;
	while (ts->ts_type == 0) {
		ts++;
		if (ts == s->stats + s->max)
			ts = s->stats;
		if (ts == s->stats + s->start)
			return NULL;
	}
	return ts;

}

static void *jbd2_seq_history_start(struct seq_file *seq, loff_t *pos)
{
	struct jbd2_stats_proc_session *s = seq->private;
	struct transaction_stats_s *ts;
	int l = *pos;

	if (l == 0)
		return SEQ_START_TOKEN;
	ts = jbd2_history_skip_empty(s, s->stats + s->start, 1);
	if (!ts)
		return NULL;
	l--;
	while (l) {
		ts = jbd2_history_skip_empty(s, ++ts, 0);
		if (!ts)
			break;
		l--;
	}
	return ts;
}

static void *jbd2_seq_history_next(struct seq_file *seq, void *v, loff_t *pos)
{
	struct jbd2_stats_proc_session *s = seq->private;
	struct transaction_stats_s *ts = v;

	++*pos;
	if (v == SEQ_START_TOKEN)
		return jbd2_history_skip_empty(s, s->stats + s->start, 1);
	else
		return jbd2_history_skip_empty(s, ++ts, 0);
}

static int jbd2_seq_history_show(struct seq_file *seq, void *v)
{
	struct transaction_stats_s *ts = v;
	if (v == SEQ_START_TOKEN) {
		seq_printf(seq, "%-4s %-5s %-5s %-5s %-5s %-5s %-5s %-6s %-5s "
				"%-5s %-5s %-5s %-5s %-5s\n", "R/C", "tid",
				"wait", "run", "lock", "flush", "log", "hndls",
				"block", "inlog", "ctime", "write", "drop",
				"close");
		return 0;
	}
	if (ts->ts_type == JBD2_STATS_RUN)
		seq_printf(seq, "%-4s %-5lu %-5u %-5u %-5u %-5u %-5u "
				"%-6lu %-5lu %-5lu\n", "R", ts->ts_tid,
				jiffies_to_msecs(ts->u.run.rs_wait),
				jiffies_to_msecs(ts->u.run.rs_running),
				jiffies_to_msecs(ts->u.run.rs_locked),
				jiffies_to_msecs(ts->u.run.rs_flushing),
				jiffies_to_msecs(ts->u.run.rs_logging),
				ts->u.run.rs_handle_count,
				ts->u.run.rs_blocks,
				ts->u.run.rs_blocks_logged);
	else if (ts->ts_type == JBD2_STATS_CHECKPOINT)
		seq_printf(seq, "%-4s %-5lu %48s %-5u %-5lu %-5lu %-5lu\n",
				"C", ts->ts_tid, " ",
				jiffies_to_msecs(ts->u.chp.cs_chp_time),
				ts->u.chp.cs_written, ts->u.chp.cs_dropped,
				ts->u.chp.cs_forced_to_close);
	else
		J_ASSERT(0);
	return 0;
}

static void jbd2_seq_history_stop(struct seq_file *seq, void *v)
{
}

static struct seq_operations jbd2_seq_history_ops = {
	.start  = jbd2_seq_history_start,
	.next   = jbd2_seq_history_next,
	.stop   = jbd2_seq_history_stop,
	.show   = jbd2_seq_history_show,
};

static int jbd2_seq_history_open(struct inode *inode, struct file *file)
{
	journal_t *journal = PDE(inode)->data;
	struct jbd2_stats_proc_session *s;
	int rc, size;

	s = kmalloc(sizeof(*s), GFP_KERNEL);
	if (s == NULL)
		return -ENOMEM;
	size = sizeof(struct transaction_stats_s) * journal->j_history_max;
	s->stats = kmalloc(size, GFP_KERNEL);
	if (s->stats == NULL) {
		kfree(s);
		return -ENOMEM;
	}
	spin_lock(&journal->j_history_lock);
	memcpy(s->stats, journal->j_history, size);
	s->max = journal->j_history_max;
	s->start = journal->j_history_cur % s->max;
	spin_unlock(&journal->j_history_lock);

	rc = seq_open(file, &jbd2_seq_history_ops);
	if (rc == 0) {
		struct seq_file *m = file->private_data;
		m->private = s;
	} else {
		kfree(s->stats);
		kfree(s);
	}
	return rc;

}

static int jbd2_seq_history_release(struct inode *inode, struct file *file)
{
	struct seq_file *seq = file->private_data;
	struct jbd2_stats_proc_session *s = seq->private;

	kfree(s->stats);
	kfree(s);
	return seq_release(inode, file);
}

static struct file_operations jbd2_seq_history_fops = {
	.owner		= THIS_MODULE,
	.open           = jbd2_seq_history_open,
	.read           = seq_read,
	.llseek         = seq_lseek,
	.release        = jbd2_seq_history_release,
};

static void *jbd2_seq_info_start(struct seq_file *seq, loff_t *pos)
{
	return *pos ? NULL : SEQ_START_TOKEN;
}

static void *jbd2_seq_info_next(struct seq_file *seq, void *v, loff_t *pos)
{
	return NULL;
}

static int jbd2_seq_info_show(struct seq_file *seq, void *v)
{
	struct jbd2_stats_proc_session *s = seq->private;

	if (v != SEQ_START_TOKEN)
		return 0;
	seq_printf(seq, "%lu transaction, each upto %u blocks\n",
			s->stats->ts_tid,
			s->journal->j_max_transaction_buffers);
	if (s->stats->ts_tid == 0)
		return 0;
	seq_printf(seq, "average: \n  %ums waiting for transaction\n",
	    jiffies_to_msecs(s->stats->u.run.rs_wait / s->stats->ts_tid));
	seq_printf(seq, "  %ums running transaction\n",
	    jiffies_to_msecs(s->stats->u.run.rs_running / s->stats->ts_tid));
	seq_printf(seq, "  %ums transaction was being locked\n",
	    jiffies_to_msecs(s->stats->u.run.rs_locked / s->stats->ts_tid));
	seq_printf(seq, "  %ums flushing data (in ordered mode)\n",
	    jiffies_to_msecs(s->stats->u.run.rs_flushing / s->stats->ts_tid));
	seq_printf(seq, "  %ums logging transaction\n",
	    jiffies_to_msecs(s->stats->u.run.rs_logging / s->stats->ts_tid));
	seq_printf(seq, "  %lluus average transaction commit time\n",
		   div_u64(s->journal->j_average_commit_time, 1000));
	seq_printf(seq, "  %lu handles per transaction\n",
	    s->stats->u.run.rs_handle_count / s->stats->ts_tid);
	seq_printf(seq, "  %lu blocks per transaction\n",
	    s->stats->u.run.rs_blocks / s->stats->ts_tid);
	seq_printf(seq, "  %lu logged blocks per transaction\n",
	    s->stats->u.run.rs_blocks_logged / s->stats->ts_tid);
	return 0;
}

static void jbd2_seq_info_stop(struct seq_file *seq, void *v)
{
}

static struct seq_operations jbd2_seq_info_ops = {
	.start  = jbd2_seq_info_start,
	.next   = jbd2_seq_info_next,
	.stop   = jbd2_seq_info_stop,
	.show   = jbd2_seq_info_show,
};

static int jbd2_seq_info_open(struct inode *inode, struct file *file)
{
	journal_t *journal = PDE(inode)->data;
	struct jbd2_stats_proc_session *s;
	int rc, size;

	s = kmalloc(sizeof(*s), GFP_KERNEL);
	if (s == NULL)
		return -ENOMEM;
	size = sizeof(struct transaction_stats_s);
	s->stats = kmalloc(size, GFP_KERNEL);
	if (s->stats == NULL) {
		kfree(s);
		return -ENOMEM;
	}
	spin_lock(&journal->j_history_lock);
	memcpy(s->stats, &journal->j_stats, size);
	s->journal = journal;
	spin_unlock(&journal->j_history_lock);

	rc = seq_open(file, &jbd2_seq_info_ops);
	if (rc == 0) {
		struct seq_file *m = file->private_data;
		m->private = s;
	} else {
		kfree(s->stats);
		kfree(s);
	}
	return rc;

}

static int jbd2_seq_info_release(struct inode *inode, struct file *file)
{
	struct seq_file *seq = file->private_data;
	struct jbd2_stats_proc_session *s = seq->private;
	kfree(s->stats);
	kfree(s);
	return seq_release(inode, file);
}

static struct file_operations jbd2_seq_info_fops = {
	.owner		= THIS_MODULE,
	.open           = jbd2_seq_info_open,
	.read           = seq_read,
	.llseek         = seq_lseek,
	.release        = jbd2_seq_info_release,
};

static struct proc_dir_entry *proc_jbd2_stats;

static void jbd2_stats_proc_init(journal_t *journal)
{
	journal->j_proc_entry = proc_mkdir(journal->j_devname, proc_jbd2_stats);
	if (journal->j_proc_entry) {
		proc_create_data("history", S_IRUGO, journal->j_proc_entry,
				 &jbd2_seq_history_fops, journal);
		proc_create_data("info", S_IRUGO, journal->j_proc_entry,
				 &jbd2_seq_info_fops, journal);
	}
}

static void jbd2_stats_proc_exit(journal_t *journal)
{
	remove_proc_entry("info", journal->j_proc_entry);
	remove_proc_entry("history", journal->j_proc_entry);
	remove_proc_entry(journal->j_devname, proc_jbd2_stats);
}

static void journal_init_stats(journal_t *journal)
{
	int size;

	if (!proc_jbd2_stats)
		return;

	journal->j_history_max = 100;
	size = sizeof(struct transaction_stats_s) * journal->j_history_max;
	journal->j_history = kzalloc(size, GFP_KERNEL);
	if (!journal->j_history) {
		journal->j_history_max = 0;
		return;
	}
	spin_lock_init(&journal->j_history_lock);
}



static journal_t * journal_init_common (void)
{
	journal_t *journal;
	int err;

	journal = kzalloc(sizeof(*journal), GFP_KERNEL|__GFP_NOFAIL);
	if (!journal)
		goto fail;

	init_waitqueue_head(&journal->j_wait_transaction_locked);
	init_waitqueue_head(&journal->j_wait_logspace);
	init_waitqueue_head(&journal->j_wait_done_commit);
	init_waitqueue_head(&journal->j_wait_checkpoint);
	init_waitqueue_head(&journal->j_wait_commit);
	init_waitqueue_head(&journal->j_wait_updates);
	mutex_init(&journal->j_barrier);
	mutex_init(&journal->j_checkpoint_mutex);
	spin_lock_init(&journal->j_revoke_lock);
	spin_lock_init(&journal->j_list_lock);
	spin_lock_init(&journal->j_state_lock);

	journal->j_commit_interval = (HZ * JBD2_DEFAULT_MAX_COMMIT_AGE);
	journal->j_min_batch_time = 0;
	journal->j_max_batch_time = 15000; /* 15ms */

	/* The journal is marked for error until we succeed with recovery! */
	journal->j_flags = JBD2_ABORT;

	/* Set up a default-sized revoke table for the new mount. */
	err = jbd2_journal_init_revoke(journal, JOURNAL_REVOKE_DEFAULT_HASH);
	if (err) {
		kfree(journal);
		goto fail;
	}

	journal_init_stats(journal);

	return journal;
fail:
	return NULL;
}


journal_t * jbd2_journal_init_dev(struct block_device *bdev,
			struct block_device *fs_dev,
			unsigned long long start, int len, int blocksize)
{
	journal_t *journal = journal_init_common();
	struct buffer_head *bh;
	char *p;
	int n;

	if (!journal)
		return NULL;

	/* journal descriptor can store up to n blocks -bzzz */
	journal->j_blocksize = blocksize;
	jbd2_stats_proc_init(journal);
	n = journal->j_blocksize / sizeof(journal_block_tag_t);
	journal->j_wbufsize = n;
	journal->j_wbuf = kmalloc(n * sizeof(struct buffer_head*), GFP_KERNEL);
	if (!journal->j_wbuf) {
		printk(KERN_ERR "%s: Cant allocate bhs for commit thread\n",
			__func__);
		goto out_err;
	}
	journal->j_dev = bdev;
	journal->j_fs_dev = fs_dev;
	journal->j_blk_offset = start;
	journal->j_maxlen = len;
	bdevname(journal->j_dev, journal->j_devname);
	p = journal->j_devname;
	while ((p = strchr(p, '/')))
		*p = '!';

	bh = __getblk(journal->j_dev, start, journal->j_blocksize);
	if (!bh) {
		printk(KERN_ERR
		       "%s: Cannot get buffer for journal superblock\n",
		       __func__);
		goto out_err;
	}
	journal->j_sb_buffer = bh;
	journal->j_superblock = (journal_superblock_t *)bh->b_data;

	return journal;
out_err:
	jbd2_stats_proc_exit(journal);
	kfree(journal);
	return NULL;
}

journal_t * jbd2_journal_init_inode (struct inode *inode)
{
	struct buffer_head *bh;
	journal_t *journal = journal_init_common();
	char *p;
	int err;
	int n;
	unsigned long long blocknr;

	if (!journal)
		return NULL;

	journal->j_dev = journal->j_fs_dev = inode->i_sb->s_bdev;
	journal->j_inode = inode;
	bdevname(journal->j_dev, journal->j_devname);
	p = journal->j_devname;
	while ((p = strchr(p, '/')))
		*p = '!';
	p = journal->j_devname + strlen(journal->j_devname);
	sprintf(p, ":%lu", journal->j_inode->i_ino);
	jbd_debug(1,
		  "journal %p: inode %s/%ld, size %Ld, bits %d, blksize %ld\n",
		  journal, inode->i_sb->s_id, inode->i_ino,
		  (long long) inode->i_size,
		  inode->i_sb->s_blocksize_bits, inode->i_sb->s_blocksize);

	journal->j_maxlen = inode->i_size >> inode->i_sb->s_blocksize_bits;
	journal->j_blocksize = inode->i_sb->s_blocksize;
	jbd2_stats_proc_init(journal);

	/* journal descriptor can store up to n blocks -bzzz */
	n = journal->j_blocksize / sizeof(journal_block_tag_t);
	journal->j_wbufsize = n;
	journal->j_wbuf = kmalloc(n * sizeof(struct buffer_head*), GFP_KERNEL);
	if (!journal->j_wbuf) {
		printk(KERN_ERR "%s: Cant allocate bhs for commit thread\n",
			__func__);
		goto out_err;
	}

	err = jbd2_journal_bmap(journal, 0, &blocknr);
	/* If that failed, give up */
	if (err) {
		printk(KERN_ERR "%s: Cannnot locate journal superblock\n",
		       __func__);
		goto out_err;
	}

	bh = __getblk(journal->j_dev, blocknr, journal->j_blocksize);
	if (!bh) {
		printk(KERN_ERR
		       "%s: Cannot get buffer for journal superblock\n",
		       __func__);
		goto out_err;
	}
	journal->j_sb_buffer = bh;
	journal->j_superblock = (journal_superblock_t *)bh->b_data;

	return journal;
out_err:
	jbd2_stats_proc_exit(journal);
	kfree(journal);
	return NULL;
}

static void journal_fail_superblock (journal_t *journal)
{
	struct buffer_head *bh = journal->j_sb_buffer;
	brelse(bh);
	journal->j_sb_buffer = NULL;
}


static int journal_reset(journal_t *journal)
{
	journal_superblock_t *sb = journal->j_superblock;
	unsigned long long first, last;

	first = be32_to_cpu(sb->s_first);
	last = be32_to_cpu(sb->s_maxlen);

	journal->j_first = first;
	journal->j_last = last;

	journal->j_head = first;
	journal->j_tail = first;
	journal->j_free = last - first;

	journal->j_tail_sequence = journal->j_transaction_sequence;
	journal->j_commit_sequence = journal->j_transaction_sequence - 1;
	journal->j_commit_request = journal->j_commit_sequence;

	journal->j_max_transaction_buffers = journal->j_maxlen / 4;

	/* Add the dynamic fields and write it to disk. */
	jbd2_journal_update_superblock(journal, 1);
	return jbd2_journal_start_thread(journal);
}

void jbd2_journal_update_superblock(journal_t *journal, int wait)
{
	journal_superblock_t *sb = journal->j_superblock;
	struct buffer_head *bh = journal->j_sb_buffer;

	/*
	 * As a special case, if the on-disk copy is already marked as needing
	 * no recovery (s_start == 0) and there are no outstanding transactions
	 * in the filesystem, then we can safely defer the superblock update
	 * until the next commit by setting JBD2_FLUSHED.  This avoids
	 * attempting a write to a potential-readonly device.
	 */
	if (sb->s_start == 0 && journal->j_tail_sequence ==
				journal->j_transaction_sequence) {
		jbd_debug(1,"JBD: Skipping superblock update on recovered sb "
			"(start %ld, seq %d, errno %d)\n",
			journal->j_tail, journal->j_tail_sequence,
			journal->j_errno);
		goto out;
	}

	if (buffer_write_io_error(bh)) {
		/*
		 * Oh, dear.  A previous attempt to write the journal
		 * superblock failed.  This could happen because the
		 * USB device was yanked out.  Or it could happen to
		 * be a transient write error and maybe the block will
		 * be remapped.  Nothing we can do but to retry the
		 * write and hope for the best.
		 */
		printk(KERN_ERR "JBD2: previous I/O error detected "
		       "for journal superblock update for %s.\n",
		       journal->j_devname);
		clear_buffer_write_io_error(bh);
		set_buffer_uptodate(bh);
	}

	spin_lock(&journal->j_state_lock);
	jbd_debug(1,"JBD: updating superblock (start %ld, seq %d, errno %d)\n",
		  journal->j_tail, journal->j_tail_sequence, journal->j_errno);

	sb->s_sequence = cpu_to_be32(journal->j_tail_sequence);
	sb->s_start    = cpu_to_be32(journal->j_tail);
	sb->s_errno    = cpu_to_be32(journal->j_errno);
	spin_unlock(&journal->j_state_lock);

	BUFFER_TRACE(bh, "marking dirty");
	mark_buffer_dirty(bh);
	if (wait) {
		sync_dirty_buffer(bh);
		if (buffer_write_io_error(bh)) {
			printk(KERN_ERR "JBD2: I/O error detected "
			       "when updating journal superblock for %s.\n",
			       journal->j_devname);
			clear_buffer_write_io_error(bh);
			set_buffer_uptodate(bh);
		}
	} else
		ll_rw_block(SWRITE, 1, &bh);

out:
	/* If we have just flushed the log (by marking s_start==0), then
	 * any future commit will have to be careful to update the
	 * superblock again to re-record the true start of the log. */

	spin_lock(&journal->j_state_lock);
	if (sb->s_start)
		journal->j_flags &= ~JBD2_FLUSHED;
	else
		journal->j_flags |= JBD2_FLUSHED;
	spin_unlock(&journal->j_state_lock);
}


static int journal_get_superblock(journal_t *journal)
{
	struct buffer_head *bh;
	journal_superblock_t *sb;
	int err = -EIO;

	bh = journal->j_sb_buffer;

	J_ASSERT(bh != NULL);
	if (!buffer_uptodate(bh)) {
		ll_rw_block(READ, 1, &bh);
		wait_on_buffer(bh);
		if (!buffer_uptodate(bh)) {
			printk (KERN_ERR
				"JBD: IO error reading journal superblock\n");
			goto out;
		}
	}

	sb = journal->j_superblock;

	err = -EINVAL;

	if (sb->s_header.h_magic != cpu_to_be32(JBD2_MAGIC_NUMBER) ||
	    sb->s_blocksize != cpu_to_be32(journal->j_blocksize)) {
		printk(KERN_WARNING "JBD: no valid journal superblock found\n");
		goto out;
	}

	switch(be32_to_cpu(sb->s_header.h_blocktype)) {
	case JBD2_SUPERBLOCK_V1:
		journal->j_format_version = 1;
		break;
	case JBD2_SUPERBLOCK_V2:
		journal->j_format_version = 2;
		break;
	default:
		printk(KERN_WARNING "JBD: unrecognised superblock format ID\n");
		goto out;
	}

	if (be32_to_cpu(sb->s_maxlen) < journal->j_maxlen)
		journal->j_maxlen = be32_to_cpu(sb->s_maxlen);
	else if (be32_to_cpu(sb->s_maxlen) > journal->j_maxlen) {
		printk (KERN_WARNING "JBD: journal file too short\n");
		goto out;
	}

	return 0;

out:
	journal_fail_superblock(journal);
	return err;
}


static int load_superblock(journal_t *journal)
{
	int err;
	journal_superblock_t *sb;

	err = journal_get_superblock(journal);
	if (err)
		return err;

	sb = journal->j_superblock;

	journal->j_tail_sequence = be32_to_cpu(sb->s_sequence);
	journal->j_tail = be32_to_cpu(sb->s_start);
	journal->j_first = be32_to_cpu(sb->s_first);
	journal->j_last = be32_to_cpu(sb->s_maxlen);
	journal->j_errno = be32_to_cpu(sb->s_errno);

	return 0;
}


int jbd2_journal_load(journal_t *journal)
{
	int err;
	journal_superblock_t *sb;

	err = load_superblock(journal);
	if (err)
		return err;

	sb = journal->j_superblock;
	/* If this is a V2 superblock, then we have to check the
	 * features flags on it. */

	if (journal->j_format_version >= 2) {
		if ((sb->s_feature_ro_compat &
		     ~cpu_to_be32(JBD2_KNOWN_ROCOMPAT_FEATURES)) ||
		    (sb->s_feature_incompat &
		     ~cpu_to_be32(JBD2_KNOWN_INCOMPAT_FEATURES))) {
			printk (KERN_WARNING
				"JBD: Unrecognised features on journal\n");
			return -EINVAL;
		}
	}

	/* Let the recovery code check whether it needs to recover any
	 * data from the journal. */
	if (jbd2_journal_recover(journal))
		goto recovery_error;

	/* OK, we've finished with the dynamic journal bits:
	 * reinitialise the dynamic contents of the superblock in memory
	 * and reset them on disk. */
	if (journal_reset(journal))
		goto recovery_error;

	journal->j_flags &= ~JBD2_ABORT;
	journal->j_flags |= JBD2_LOADED;
	return 0;

recovery_error:
	printk (KERN_WARNING "JBD: recovery failed\n");
	return -EIO;
}

int jbd2_journal_destroy(journal_t *journal)
{
	int err = 0;

	/* Wait for the commit thread to wake up and die. */
	journal_kill_thread(journal);

	/* Force a final log commit */
	if (journal->j_running_transaction)
		jbd2_journal_commit_transaction(journal);

	/* Force any old transactions to disk */

	/* Totally anal locking here... */
	spin_lock(&journal->j_list_lock);
	while (journal->j_checkpoint_transactions != NULL) {
		spin_unlock(&journal->j_list_lock);
		mutex_lock(&journal->j_checkpoint_mutex);
		jbd2_log_do_checkpoint(journal);
		mutex_unlock(&journal->j_checkpoint_mutex);
		spin_lock(&journal->j_list_lock);
	}

	J_ASSERT(journal->j_running_transaction == NULL);
	J_ASSERT(journal->j_committing_transaction == NULL);
	J_ASSERT(journal->j_checkpoint_transactions == NULL);
	spin_unlock(&journal->j_list_lock);

	if (journal->j_sb_buffer) {
		if (!is_journal_aborted(journal)) {
			/* We can now mark the journal as empty. */
			journal->j_tail = 0;
			journal->j_tail_sequence =
				++journal->j_transaction_sequence;
			jbd2_journal_update_superblock(journal, 1);
		} else {
			err = -EIO;
		}
		brelse(journal->j_sb_buffer);
	}

	if (journal->j_proc_entry)
		jbd2_stats_proc_exit(journal);
	if (journal->j_inode)
		iput(journal->j_inode);
	if (journal->j_revoke)
		jbd2_journal_destroy_revoke(journal);
	kfree(journal->j_wbuf);
	kfree(journal);

	return err;
}



int jbd2_journal_check_used_features (journal_t *journal, unsigned long compat,
				 unsigned long ro, unsigned long incompat)
{
	journal_superblock_t *sb;

	if (!compat && !ro && !incompat)
		return 1;
	if (journal->j_format_version == 1)
		return 0;

	sb = journal->j_superblock;

	if (((be32_to_cpu(sb->s_feature_compat) & compat) == compat) &&
	    ((be32_to_cpu(sb->s_feature_ro_compat) & ro) == ro) &&
	    ((be32_to_cpu(sb->s_feature_incompat) & incompat) == incompat))
		return 1;

	return 0;
}


int jbd2_journal_check_available_features (journal_t *journal, unsigned long compat,
				      unsigned long ro, unsigned long incompat)
{
	journal_superblock_t *sb;

	if (!compat && !ro && !incompat)
		return 1;

	sb = journal->j_superblock;

	/* We can support any known requested features iff the
	 * superblock is in version 2.  Otherwise we fail to support any
	 * extended sb features. */

	if (journal->j_format_version != 2)
		return 0;

	if ((compat   & JBD2_KNOWN_COMPAT_FEATURES) == compat &&
	    (ro       & JBD2_KNOWN_ROCOMPAT_FEATURES) == ro &&
	    (incompat & JBD2_KNOWN_INCOMPAT_FEATURES) == incompat)
		return 1;

	return 0;
}


int jbd2_journal_set_features (journal_t *journal, unsigned long compat,
			  unsigned long ro, unsigned long incompat)
{
	journal_superblock_t *sb;

	if (jbd2_journal_check_used_features(journal, compat, ro, incompat))
		return 1;

	if (!jbd2_journal_check_available_features(journal, compat, ro, incompat))
		return 0;

	jbd_debug(1, "Setting new features 0x%lx/0x%lx/0x%lx\n",
		  compat, ro, incompat);

	sb = journal->j_superblock;

	sb->s_feature_compat    |= cpu_to_be32(compat);
	sb->s_feature_ro_compat |= cpu_to_be32(ro);
	sb->s_feature_incompat  |= cpu_to_be32(incompat);

	return 1;
}

void jbd2_journal_clear_features(journal_t *journal, unsigned long compat,
				unsigned long ro, unsigned long incompat)
{
	journal_superblock_t *sb;

	jbd_debug(1, "Clear features 0x%lx/0x%lx/0x%lx\n",
		  compat, ro, incompat);

	sb = journal->j_superblock;

	sb->s_feature_compat    &= ~cpu_to_be32(compat);
	sb->s_feature_ro_compat &= ~cpu_to_be32(ro);
	sb->s_feature_incompat  &= ~cpu_to_be32(incompat);
}
EXPORT_SYMBOL(jbd2_journal_clear_features);

int jbd2_journal_update_format (journal_t *journal)
{
	journal_superblock_t *sb;
	int err;

	err = journal_get_superblock(journal);
	if (err)
		return err;

	sb = journal->j_superblock;

	switch (be32_to_cpu(sb->s_header.h_blocktype)) {
	case JBD2_SUPERBLOCK_V2:
		return 0;
	case JBD2_SUPERBLOCK_V1:
		return journal_convert_superblock_v1(journal, sb);
	default:
		break;
	}
	return -EINVAL;
}

static int journal_convert_superblock_v1(journal_t *journal,
					 journal_superblock_t *sb)
{
	int offset, blocksize;
	struct buffer_head *bh;

	printk(KERN_WARNING
		"JBD: Converting superblock from version 1 to 2.\n");

	/* Pre-initialise new fields to zero */
	offset = ((char *) &(sb->s_feature_compat)) - ((char *) sb);
	blocksize = be32_to_cpu(sb->s_blocksize);
	memset(&sb->s_feature_compat, 0, blocksize-offset);

	sb->s_nr_users = cpu_to_be32(1);
	sb->s_header.h_blocktype = cpu_to_be32(JBD2_SUPERBLOCK_V2);
	journal->j_format_version = 2;

	bh = journal->j_sb_buffer;
	BUFFER_TRACE(bh, "marking dirty");
	mark_buffer_dirty(bh);
	sync_dirty_buffer(bh);
	return 0;
}



int jbd2_journal_flush(journal_t *journal)
{
	int err = 0;
	transaction_t *transaction = NULL;
	unsigned long old_tail;

	spin_lock(&journal->j_state_lock);

	/* Force everything buffered to the log... */
	if (journal->j_running_transaction) {
		transaction = journal->j_running_transaction;
		__jbd2_log_start_commit(journal, transaction->t_tid);
	} else if (journal->j_committing_transaction)
		transaction = journal->j_committing_transaction;

	/* Wait for the log commit to complete... */
	if (transaction) {
		tid_t tid = transaction->t_tid;

		spin_unlock(&journal->j_state_lock);
		jbd2_log_wait_commit(journal, tid);
	} else {
		spin_unlock(&journal->j_state_lock);
	}

	/* ...and flush everything in the log out to disk. */
	spin_lock(&journal->j_list_lock);
	while (!err && journal->j_checkpoint_transactions != NULL) {
		spin_unlock(&journal->j_list_lock);
		mutex_lock(&journal->j_checkpoint_mutex);
		err = jbd2_log_do_checkpoint(journal);
		mutex_unlock(&journal->j_checkpoint_mutex);
		spin_lock(&journal->j_list_lock);
	}
	spin_unlock(&journal->j_list_lock);

	if (is_journal_aborted(journal))
		return -EIO;

	jbd2_cleanup_journal_tail(journal);

	/* Finally, mark the journal as really needing no recovery.
	 * This sets s_start==0 in the underlying superblock, which is
	 * the magic code for a fully-recovered superblock.  Any future
	 * commits of data to the journal will restore the current
	 * s_start value. */
	spin_lock(&journal->j_state_lock);
	old_tail = journal->j_tail;
	journal->j_tail = 0;
	spin_unlock(&journal->j_state_lock);
	jbd2_journal_update_superblock(journal, 1);
	spin_lock(&journal->j_state_lock);
	journal->j_tail = old_tail;

	J_ASSERT(!journal->j_running_transaction);
	J_ASSERT(!journal->j_committing_transaction);
	J_ASSERT(!journal->j_checkpoint_transactions);
	J_ASSERT(journal->j_head == journal->j_tail);
	J_ASSERT(journal->j_tail_sequence == journal->j_transaction_sequence);
	spin_unlock(&journal->j_state_lock);
	return 0;
}


int jbd2_journal_wipe(journal_t *journal, int write)
{
	journal_superblock_t *sb;
	int err = 0;

	J_ASSERT (!(journal->j_flags & JBD2_LOADED));

	err = load_superblock(journal);
	if (err)
		return err;

	sb = journal->j_superblock;

	if (!journal->j_tail)
		goto no_recovery;

	printk (KERN_WARNING "JBD: %s recovery information on journal\n",
		write ? "Clearing" : "Ignoring");

	err = jbd2_journal_skip_recovery(journal);
	if (write)
		jbd2_journal_update_superblock(journal, 1);

 no_recovery:
	return err;
}


void __jbd2_journal_abort_hard(journal_t *journal)
{
	transaction_t *transaction;

	if (journal->j_flags & JBD2_ABORT)
		return;

	printk(KERN_ERR "Aborting journal on device %s.\n",
	       journal->j_devname);

	spin_lock(&journal->j_state_lock);
	journal->j_flags |= JBD2_ABORT;
	transaction = journal->j_running_transaction;
	if (transaction)
		__jbd2_log_start_commit(journal, transaction->t_tid);
	spin_unlock(&journal->j_state_lock);
}

static void __journal_abort_soft (journal_t *journal, int errno)
{
	if (journal->j_flags & JBD2_ABORT)
		return;

	if (!journal->j_errno)
		journal->j_errno = errno;

	__jbd2_journal_abort_hard(journal);

	if (errno)
		jbd2_journal_update_superblock(journal, 1);
}


void jbd2_journal_abort(journal_t *journal, int errno)
{
	__journal_abort_soft(journal, errno);
}

int jbd2_journal_errno(journal_t *journal)
{
	int err;

	spin_lock(&journal->j_state_lock);
	if (journal->j_flags & JBD2_ABORT)
		err = -EROFS;
	else
		err = journal->j_errno;
	spin_unlock(&journal->j_state_lock);
	return err;
}

int jbd2_journal_clear_err(journal_t *journal)
{
	int err = 0;

	spin_lock(&journal->j_state_lock);
	if (journal->j_flags & JBD2_ABORT)
		err = -EROFS;
	else
		journal->j_errno = 0;
	spin_unlock(&journal->j_state_lock);
	return err;
}

void jbd2_journal_ack_err(journal_t *journal)
{
	spin_lock(&journal->j_state_lock);
	if (journal->j_errno)
		journal->j_flags |= JBD2_ACK_ERR;
	spin_unlock(&journal->j_state_lock);
}

int jbd2_journal_blocks_per_page(struct inode *inode)
{
	return 1 << (PAGE_CACHE_SHIFT - inode->i_sb->s_blocksize_bits);
}

size_t journal_tag_bytes(journal_t *journal)
{
	if (JBD2_HAS_INCOMPAT_FEATURE(journal, JBD2_FEATURE_INCOMPAT_64BIT))
		return JBD2_TAG_SIZE64;
	else
		return JBD2_TAG_SIZE32;
}

static struct kmem_cache *jbd2_journal_head_cache;
#ifdef CONFIG_JBD2_DEBUG
static atomic_t nr_journal_heads = ATOMIC_INIT(0);
#endif

static int journal_init_jbd2_journal_head_cache(void)
{
	int retval;

	J_ASSERT(jbd2_journal_head_cache == NULL);
	jbd2_journal_head_cache = kmem_cache_create("jbd2_journal_head",
				sizeof(struct journal_head),
				0,		/* offset */
				SLAB_TEMPORARY,	/* flags */
				NULL);		/* ctor */
	retval = 0;
	if (!jbd2_journal_head_cache) {
		retval = -ENOMEM;
		printk(KERN_EMERG "JBD: no memory for journal_head cache\n");
	}
	return retval;
}

static void jbd2_journal_destroy_jbd2_journal_head_cache(void)
{
	if (jbd2_journal_head_cache) {
		kmem_cache_destroy(jbd2_journal_head_cache);
		jbd2_journal_head_cache = NULL;
	}
}

static struct journal_head *journal_alloc_journal_head(void)
{
	struct journal_head *ret;
	static unsigned long last_warning;

#ifdef CONFIG_JBD2_DEBUG
	atomic_inc(&nr_journal_heads);
#endif
	ret = kmem_cache_alloc(jbd2_journal_head_cache, GFP_NOFS);
	if (!ret) {
		jbd_debug(1, "out of memory for journal_head\n");
		if (time_after(jiffies, last_warning + 5*HZ)) {
			printk(KERN_NOTICE "ENOMEM in %s, retrying.\n",
			       __func__);
			last_warning = jiffies;
		}
		while (!ret) {
			yield();
			ret = kmem_cache_alloc(jbd2_journal_head_cache, GFP_NOFS);
		}
	}
	return ret;
}

static void journal_free_journal_head(struct journal_head *jh)
{
#ifdef CONFIG_JBD2_DEBUG
	atomic_dec(&nr_journal_heads);
	memset(jh, JBD2_POISON_FREE, sizeof(*jh));
#endif
	kmem_cache_free(jbd2_journal_head_cache, jh);
}


struct journal_head *jbd2_journal_add_journal_head(struct buffer_head *bh)
{
	struct journal_head *jh;
	struct journal_head *new_jh = NULL;

repeat:
	if (!buffer_jbd(bh)) {
		new_jh = journal_alloc_journal_head();
		memset(new_jh, 0, sizeof(*new_jh));
	}

	jbd_lock_bh_journal_head(bh);
	if (buffer_jbd(bh)) {
		jh = bh2jh(bh);
	} else {
		J_ASSERT_BH(bh,
			(atomic_read(&bh->b_count) > 0) ||
			(bh->b_page && bh->b_page->mapping));

		if (!new_jh) {
			jbd_unlock_bh_journal_head(bh);
			goto repeat;
		}

		jh = new_jh;
		new_jh = NULL;		/* We consumed it */
		set_buffer_jbd(bh);
		bh->b_private = jh;
		jh->b_bh = bh;
		get_bh(bh);
		BUFFER_TRACE(bh, "added journal_head");
	}
	jh->b_jcount++;
	jbd_unlock_bh_journal_head(bh);
	if (new_jh)
		journal_free_journal_head(new_jh);
	return bh->b_private;
}

struct journal_head *jbd2_journal_grab_journal_head(struct buffer_head *bh)
{
	struct journal_head *jh = NULL;

	jbd_lock_bh_journal_head(bh);
	if (buffer_jbd(bh)) {
		jh = bh2jh(bh);
		jh->b_jcount++;
	}
	jbd_unlock_bh_journal_head(bh);
	return jh;
}

static void __journal_remove_journal_head(struct buffer_head *bh)
{
	struct journal_head *jh = bh2jh(bh);

	J_ASSERT_JH(jh, jh->b_jcount >= 0);

	get_bh(bh);
	if (jh->b_jcount == 0) {
		if (jh->b_transaction == NULL &&
				jh->b_next_transaction == NULL &&
				jh->b_cp_transaction == NULL) {
			J_ASSERT_JH(jh, jh->b_jlist == BJ_None);
			J_ASSERT_BH(bh, buffer_jbd(bh));
			J_ASSERT_BH(bh, jh2bh(jh) == bh);
			BUFFER_TRACE(bh, "remove journal_head");
			if (jh->b_frozen_data) {
				printk(KERN_WARNING "%s: freeing "
						"b_frozen_data\n",
						__func__);
				jbd2_free(jh->b_frozen_data, bh->b_size);
			}
			if (jh->b_committed_data) {
				printk(KERN_WARNING "%s: freeing "
						"b_committed_data\n",
						__func__);
				jbd2_free(jh->b_committed_data, bh->b_size);
			}
			bh->b_private = NULL;
			jh->b_bh = NULL;	/* debug, really */
			clear_buffer_jbd(bh);
			__brelse(bh);
			journal_free_journal_head(jh);
		} else {
			BUFFER_TRACE(bh, "journal_head was locked");
		}
	}
}

void jbd2_journal_remove_journal_head(struct buffer_head *bh)
{
	jbd_lock_bh_journal_head(bh);
	__journal_remove_journal_head(bh);
	jbd_unlock_bh_journal_head(bh);
}

void jbd2_journal_put_journal_head(struct journal_head *jh)
{
	struct buffer_head *bh = jh2bh(jh);

	jbd_lock_bh_journal_head(bh);
	J_ASSERT_JH(jh, jh->b_jcount > 0);
	--jh->b_jcount;
	if (!jh->b_jcount && !jh->b_transaction) {
		__journal_remove_journal_head(bh);
		__brelse(bh);
	}
	jbd_unlock_bh_journal_head(bh);
}

void jbd2_journal_init_jbd_inode(struct jbd2_inode *jinode, struct inode *inode)
{
	jinode->i_transaction = NULL;
	jinode->i_next_transaction = NULL;
	jinode->i_vfs_inode = inode;
	jinode->i_flags = 0;
	INIT_LIST_HEAD(&jinode->i_list);
}

void jbd2_journal_release_jbd_inode(journal_t *journal,
				    struct jbd2_inode *jinode)
{
	int writeout = 0;

	if (!journal)
		return;
restart:
	spin_lock(&journal->j_list_lock);
	/* Is commit writing out inode - we have to wait */
	if (jinode->i_flags & JI_COMMIT_RUNNING) {
		wait_queue_head_t *wq;
		DEFINE_WAIT_BIT(wait, &jinode->i_flags, __JI_COMMIT_RUNNING);
		wq = bit_waitqueue(&jinode->i_flags, __JI_COMMIT_RUNNING);
		prepare_to_wait(wq, &wait.wait, TASK_UNINTERRUPTIBLE);
		spin_unlock(&journal->j_list_lock);
		schedule();
		finish_wait(wq, &wait.wait);
		goto restart;
	}

	/* Do we need to wait for data writeback? */
	if (journal->j_committing_transaction == jinode->i_transaction)
		writeout = 1;
	if (jinode->i_transaction) {
		list_del(&jinode->i_list);
		jinode->i_transaction = NULL;
	}
	spin_unlock(&journal->j_list_lock);
}

#ifdef CONFIG_JBD2_DEBUG
u8 jbd2_journal_enable_debug __read_mostly;
EXPORT_SYMBOL(jbd2_journal_enable_debug);

#define JBD2_DEBUG_NAME "jbd2-debug"

static struct dentry *jbd2_debugfs_dir;
static struct dentry *jbd2_debug;

static void __init jbd2_create_debugfs_entry(void)
{
	jbd2_debugfs_dir = debugfs_create_dir("jbd2", NULL);
	if (jbd2_debugfs_dir)
		jbd2_debug = debugfs_create_u8(JBD2_DEBUG_NAME, S_IRUGO,
					       jbd2_debugfs_dir,
					       &jbd2_journal_enable_debug);
}

static void __exit jbd2_remove_debugfs_entry(void)
{
	debugfs_remove(jbd2_debug);
	debugfs_remove(jbd2_debugfs_dir);
}

#else

static void __init jbd2_create_debugfs_entry(void)
{
}

static void __exit jbd2_remove_debugfs_entry(void)
{
}

#endif

#ifdef CONFIG_PROC_FS

#define JBD2_STATS_PROC_NAME "fs/jbd2"

static void __init jbd2_create_jbd_stats_proc_entry(void)
{
	proc_jbd2_stats = proc_mkdir(JBD2_STATS_PROC_NAME, NULL);
}

static void __exit jbd2_remove_jbd_stats_proc_entry(void)
{
	if (proc_jbd2_stats)
		remove_proc_entry(JBD2_STATS_PROC_NAME, NULL);
}

#else

#define jbd2_create_jbd_stats_proc_entry() do {} while (0)
#define jbd2_remove_jbd_stats_proc_entry() do {} while (0)

#endif

struct kmem_cache *jbd2_handle_cache;

static int __init journal_init_handle_cache(void)
{
	jbd2_handle_cache = kmem_cache_create("jbd2_journal_handle",
				sizeof(handle_t),
				0,		/* offset */
				SLAB_TEMPORARY,	/* flags */
				NULL);		/* ctor */
	if (jbd2_handle_cache == NULL) {
		printk(KERN_EMERG "JBD: failed to create handle cache\n");
		return -ENOMEM;
	}
	return 0;
}

static void jbd2_journal_destroy_handle_cache(void)
{
	if (jbd2_handle_cache)
		kmem_cache_destroy(jbd2_handle_cache);
}


static int __init journal_init_caches(void)
{
	int ret;

	ret = jbd2_journal_init_revoke_caches();
	if (ret == 0)
		ret = journal_init_jbd2_journal_head_cache();
	if (ret == 0)
		ret = journal_init_handle_cache();
	return ret;
}

static void jbd2_journal_destroy_caches(void)
{
	jbd2_journal_destroy_revoke_caches();
	jbd2_journal_destroy_jbd2_journal_head_cache();
	jbd2_journal_destroy_handle_cache();
}

static int __init journal_init(void)
{
	int ret;

	BUILD_BUG_ON(sizeof(struct journal_superblock_s) != 1024);

	ret = journal_init_caches();
	if (ret == 0) {
		jbd2_create_debugfs_entry();
		jbd2_create_jbd_stats_proc_entry();
	} else {
		jbd2_journal_destroy_caches();
	}
	return ret;
}

static void __exit journal_exit(void)
{
#ifdef CONFIG_JBD2_DEBUG
	int n = atomic_read(&nr_journal_heads);
	if (n)
		printk(KERN_EMERG "JBD: leaked %d journal_heads!\n", n);
#endif
	jbd2_remove_debugfs_entry();
	jbd2_remove_jbd_stats_proc_entry();
	jbd2_journal_destroy_caches();
}

MODULE_LICENSE("GPL");
module_init(journal_init);
module_exit(journal_exit);

