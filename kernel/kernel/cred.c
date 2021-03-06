
#include <linux/module.h>
#include <linux/cred.h>
#include <linux/sched.h>
#include <linux/key.h>
#include <linux/keyctl.h>
#include <linux/init_task.h>
#include <linux/security.h>
#include <linux/cn_proc.h>
#include "cred-internals.h"

static struct kmem_cache *cred_jar;

#ifdef CONFIG_KEYS
static struct thread_group_cred init_tgcred = {
	.usage	= ATOMIC_INIT(2),
	.tgid	= 0,
	.lock	= SPIN_LOCK_UNLOCKED,
};
#endif

struct cred init_cred = {
	.usage			= ATOMIC_INIT(4),
	.securebits		= SECUREBITS_DEFAULT,
	.cap_inheritable	= CAP_INIT_INH_SET,
	.cap_permitted		= CAP_FULL_SET,
	.cap_effective		= CAP_INIT_EFF_SET,
	.cap_bset		= CAP_INIT_BSET,
	.user			= INIT_USER,
	.group_info		= &init_groups,
#ifdef CONFIG_KEYS
	.tgcred			= &init_tgcred,
#endif
};

#ifdef CONFIG_KEYS
static void release_tgcred_rcu(struct rcu_head *rcu)
{
	struct thread_group_cred *tgcred =
		container_of(rcu, struct thread_group_cred, rcu);

	BUG_ON(atomic_read(&tgcred->usage) != 0);

	key_put(tgcred->session_keyring);
	key_put(tgcred->process_keyring);
	kfree(tgcred);
}
#endif

static void release_tgcred(struct cred *cred)
{
#ifdef CONFIG_KEYS
	struct thread_group_cred *tgcred = cred->tgcred;

	if (atomic_dec_and_test(&tgcred->usage))
		call_rcu(&tgcred->rcu, release_tgcred_rcu);
#endif
}

static void put_cred_rcu(struct rcu_head *rcu)
{
	struct cred *cred = container_of(rcu, struct cred, rcu);

	if (atomic_read(&cred->usage) != 0)
		panic("CRED: put_cred_rcu() sees %p with usage %d\n",
		      cred, atomic_read(&cred->usage));

	security_cred_free(cred);
	key_put(cred->thread_keyring);
	key_put(cred->request_key_auth);
	release_tgcred(cred);
	put_group_info(cred->group_info);
	free_uid(cred->user);
	kmem_cache_free(cred_jar, cred);
}

void __put_cred(struct cred *cred)
{
	BUG_ON(atomic_read(&cred->usage) != 0);

	call_rcu(&cred->rcu, put_cred_rcu);
}
EXPORT_SYMBOL(__put_cred);

struct cred *prepare_creds(void)
{
	struct task_struct *task = current;
	const struct cred *old;
	struct cred *new;

	BUG_ON(atomic_read(&task->real_cred->usage) < 1);

	new = kmem_cache_alloc(cred_jar, GFP_KERNEL);
	if (!new)
		return NULL;

	old = task->cred;
	memcpy(new, old, sizeof(struct cred));

	atomic_set(&new->usage, 1);
	get_group_info(new->group_info);
	get_uid(new->user);

#ifdef CONFIG_KEYS
	key_get(new->thread_keyring);
	key_get(new->request_key_auth);
	atomic_inc(&new->tgcred->usage);
#endif

#ifdef CONFIG_SECURITY
	new->security = NULL;
#endif

	if (security_prepare_creds(new, old, GFP_KERNEL) < 0)
		goto error;
	return new;

error:
	abort_creds(new);
	return NULL;
}
EXPORT_SYMBOL(prepare_creds);

struct cred *prepare_exec_creds(void)
{
	struct thread_group_cred *tgcred = NULL;
	struct cred *new;

#ifdef CONFIG_KEYS
	tgcred = kmalloc(sizeof(*tgcred), GFP_KERNEL);
	if (!tgcred)
		return NULL;
#endif

	new = prepare_creds();
	if (!new) {
		kfree(tgcred);
		return new;
	}

#ifdef CONFIG_KEYS
	/* newly exec'd tasks don't get a thread keyring */
	key_put(new->thread_keyring);
	new->thread_keyring = NULL;

	/* create a new per-thread-group creds for all this set of threads to
	 * share */
	memcpy(tgcred, new->tgcred, sizeof(struct thread_group_cred));

	atomic_set(&tgcred->usage, 1);
	spin_lock_init(&tgcred->lock);

	/* inherit the session keyring; new process keyring */
	key_get(tgcred->session_keyring);
	tgcred->process_keyring = NULL;

	release_tgcred(new);
	new->tgcred = tgcred;
#endif

	return new;
}

struct cred *prepare_usermodehelper_creds(void)
{
#ifdef CONFIG_KEYS
	struct thread_group_cred *tgcred = NULL;
#endif
	struct cred *new;

#ifdef CONFIG_KEYS
	tgcred = kzalloc(sizeof(*new->tgcred), GFP_ATOMIC);
	if (!tgcred)
		return NULL;
#endif

	new = kmem_cache_alloc(cred_jar, GFP_ATOMIC);
	if (!new)
		return NULL;

	memcpy(new, &init_cred, sizeof(struct cred));

	atomic_set(&new->usage, 1);
	get_group_info(new->group_info);
	get_uid(new->user);

#ifdef CONFIG_KEYS
	new->thread_keyring = NULL;
	new->request_key_auth = NULL;
	new->jit_keyring = KEY_REQKEY_DEFL_DEFAULT;

	atomic_set(&tgcred->usage, 1);
	spin_lock_init(&tgcred->lock);
	new->tgcred = tgcred;
#endif

#ifdef CONFIG_SECURITY
	new->security = NULL;
#endif
	if (security_prepare_creds(new, &init_cred, GFP_ATOMIC) < 0)
		goto error;

	BUG_ON(atomic_read(&new->usage) != 1);
	return new;

error:
	put_cred(new);
	return NULL;
}

int copy_creds(struct task_struct *p, unsigned long clone_flags)
{
#ifdef CONFIG_KEYS
	struct thread_group_cred *tgcred;
#endif
	struct cred *new;
	int ret;

	mutex_init(&p->cred_exec_mutex);

	if (
#ifdef CONFIG_KEYS
		!p->cred->thread_keyring &&
#endif
		clone_flags & CLONE_THREAD
	    ) {
		p->real_cred = get_cred(p->cred);
		get_cred(p->cred);
		atomic_inc(&p->cred->user->processes);
		return 0;
	}

	new = prepare_creds();
	if (!new)
		return -ENOMEM;

	if (clone_flags & CLONE_NEWUSER) {
		ret = create_user_ns(new);
		if (ret < 0)
			goto error_put;
	}

#ifdef CONFIG_KEYS
	/* new threads get their own thread keyrings if their parent already
	 * had one */
	if (new->thread_keyring) {
		key_put(new->thread_keyring);
		new->thread_keyring = NULL;
		if (clone_flags & CLONE_THREAD)
			install_thread_keyring_to_cred(new);
	}

	/* we share the process and session keyrings between all the threads in
	 * a process - this is slightly icky as we violate COW credentials a
	 * bit */
	if (!(clone_flags & CLONE_THREAD)) {
		tgcred = kmalloc(sizeof(*tgcred), GFP_KERNEL);
		if (!tgcred) {
			ret = -ENOMEM;
			goto error_put;
		}
		atomic_set(&tgcred->usage, 1);
		spin_lock_init(&tgcred->lock);
		tgcred->process_keyring = NULL;
		tgcred->session_keyring = key_get(new->tgcred->session_keyring);

		release_tgcred(new);
		new->tgcred = tgcred;
	}
#endif

	atomic_inc(&new->user->processes);
	p->cred = p->real_cred = get_cred(new);
	return 0;

error_put:
	put_cred(new);
	return ret;
}

int commit_creds(struct cred *new)
{
	struct task_struct *task = current;
	const struct cred *old;

	BUG_ON(task->cred != task->real_cred);
	BUG_ON(atomic_read(&task->real_cred->usage) < 2);
	BUG_ON(atomic_read(&new->usage) < 1);

	old = task->real_cred;
	security_commit_creds(new, old);

	get_cred(new); /* we will require a ref for the subj creds too */

	/* dumpability changes */
	if (old->euid != new->euid ||
	    old->egid != new->egid ||
	    old->fsuid != new->fsuid ||
	    old->fsgid != new->fsgid ||
	    !cap_issubset(new->cap_permitted, old->cap_permitted)) {
		if (task->mm)
			set_dumpable(task->mm, suid_dumpable);
		task->pdeath_signal = 0;
		smp_wmb();
	}

	/* alter the thread keyring */
	if (new->fsuid != old->fsuid)
		key_fsuid_changed(task);
	if (new->fsgid != old->fsgid)
		key_fsgid_changed(task);

	/* do it
	 * - What if a process setreuid()'s and this brings the
	 *   new uid over his NPROC rlimit?  We can check this now
	 *   cheaply with the new uid cache, so if it matters
	 *   we should be checking for it.  -DaveM
	 */
	if (new->user != old->user)
		atomic_inc(&new->user->processes);
	rcu_assign_pointer(task->real_cred, new);
	rcu_assign_pointer(task->cred, new);
	if (new->user != old->user)
		atomic_dec(&old->user->processes);

	sched_switch_user(task);

	/* send notifications */
	if (new->uid   != old->uid  ||
	    new->euid  != old->euid ||
	    new->suid  != old->suid ||
	    new->fsuid != old->fsuid)
		proc_id_connector(task, PROC_EVENT_UID);

	if (new->gid   != old->gid  ||
	    new->egid  != old->egid ||
	    new->sgid  != old->sgid ||
	    new->fsgid != old->fsgid)
		proc_id_connector(task, PROC_EVENT_GID);

	/* release the old obj and subj refs both */
	put_cred(old);
	put_cred(old);
	return 0;
}
EXPORT_SYMBOL(commit_creds);

void abort_creds(struct cred *new)
{
	BUG_ON(atomic_read(&new->usage) < 1);
	put_cred(new);
}
EXPORT_SYMBOL(abort_creds);

const struct cred *override_creds(const struct cred *new)
{
	const struct cred *old = current->cred;

	rcu_assign_pointer(current->cred, get_cred(new));
	return old;
}
EXPORT_SYMBOL(override_creds);

void revert_creds(const struct cred *old)
{
	const struct cred *override = current->cred;

	rcu_assign_pointer(current->cred, old);
	put_cred(override);
}
EXPORT_SYMBOL(revert_creds);

void __init cred_init(void)
{
	/* allocate a slab in which we can store credentials */
	cred_jar = kmem_cache_create("cred_jar", sizeof(struct cred),
				     0, SLAB_HWCACHE_ALIGN|SLAB_PANIC, NULL);
}

struct cred *prepare_kernel_cred(struct task_struct *daemon)
{
	const struct cred *old;
	struct cred *new;

	new = kmem_cache_alloc(cred_jar, GFP_KERNEL);
	if (!new)
		return NULL;

	if (daemon)
		old = get_task_cred(daemon);
	else
		old = get_cred(&init_cred);

	*new = *old;
	get_uid(new->user);
	get_group_info(new->group_info);

#ifdef CONFIG_KEYS
	atomic_inc(&init_tgcred.usage);
	new->tgcred = &init_tgcred;
	new->request_key_auth = NULL;
	new->thread_keyring = NULL;
	new->jit_keyring = KEY_REQKEY_DEFL_THREAD_KEYRING;
#endif

#ifdef CONFIG_SECURITY
	new->security = NULL;
#endif
	if (security_prepare_creds(new, old, GFP_KERNEL) < 0)
		goto error;

	atomic_set(&new->usage, 1);
	put_cred(old);
	return new;

error:
	put_cred(new);
	put_cred(old);
	return NULL;
}
EXPORT_SYMBOL(prepare_kernel_cred);

int set_security_override(struct cred *new, u32 secid)
{
	return security_kernel_act_as(new, secid);
}
EXPORT_SYMBOL(set_security_override);

int set_security_override_from_ctx(struct cred *new, const char *secctx)
{
	u32 secid;
	int ret;

	ret = security_secctx_to_secid(secctx, strlen(secctx), &secid);
	if (ret < 0)
		return ret;

	return set_security_override(new, secid);
}
EXPORT_SYMBOL(set_security_override_from_ctx);

int set_create_files_as(struct cred *new, struct inode *inode)
{
	new->fsuid = inode->i_uid;
	new->fsgid = inode->i_gid;
	return security_kernel_create_files_as(new, inode);
}
EXPORT_SYMBOL(set_create_files_as);
