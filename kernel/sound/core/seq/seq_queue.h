
#ifndef __SND_SEQ_QUEUE_H
#define __SND_SEQ_QUEUE_H

#include "seq_memory.h"
#include "seq_prioq.h"
#include "seq_timer.h"
#include "seq_lock.h"
#include <linux/interrupt.h>
#include <linux/list.h>
#include <linux/bitops.h>

#define SEQ_QUEUE_NO_OWNER (-1)

struct snd_seq_queue {
	int queue;		/* queue number */

	char name[64];		/* name of this queue */

	struct snd_seq_prioq	*tickq;		/* midi tick event queue */
	struct snd_seq_prioq	*timeq;		/* real-time event queue */	
	
	struct snd_seq_timer *timer;	/* time keeper for this queue */
	int	owner;		/* client that 'owns' the timer */
	unsigned int	locked:1,	/* timer is only accesibble by owner if set */
		klocked:1,	/* kernel lock (after START) */	
		check_again:1,
		check_blocked:1;

	unsigned int flags;		/* status flags */
	unsigned int info_flags;	/* info for sync */

	spinlock_t owner_lock;
	spinlock_t check_lock;

	/* clients which uses this queue (bitmap) */
	DECLARE_BITMAP(clients_bitmap, SNDRV_SEQ_MAX_CLIENTS);
	unsigned int clients;	/* users of this queue */
	struct mutex timer_mutex;

	snd_use_lock_t use_lock;
};


/* get the number of current queues */
int snd_seq_queue_get_cur_queues(void);

/* init queues structure */
int snd_seq_queues_init(void);

/* delete queues */ 
void snd_seq_queues_delete(void);


/* create new queue (constructor) */
int snd_seq_queue_alloc(int client, int locked, unsigned int flags);

/* delete queue (destructor) */
int snd_seq_queue_delete(int client, int queueid);

/* notification that client has left the system */
void snd_seq_queue_client_termination(int client);

/* final stage */
void snd_seq_queue_client_leave(int client);

/* enqueue a event received from one the clients */
int snd_seq_enqueue_event(struct snd_seq_event_cell *cell, int atomic, int hop);

/* Remove events */
void snd_seq_queue_client_leave_cells(int client);
void snd_seq_queue_remove_cells(int client, struct snd_seq_remove_events *info);

/* return pointer to queue structure for specified id */
struct snd_seq_queue *queueptr(int queueid);
/* unlock */
#define queuefree(q) snd_use_lock_free(&(q)->use_lock)

/* return the (first) queue matching with the specified name */
struct snd_seq_queue *snd_seq_queue_find_name(char *name);

/* check single queue and dispatch events */
void snd_seq_check_queue(struct snd_seq_queue *q, int atomic, int hop);

/* access to queue's parameters */
int snd_seq_queue_check_access(int queueid, int client);
int snd_seq_queue_timer_set_tempo(int queueid, int client, struct snd_seq_queue_tempo *info);
int snd_seq_queue_set_owner(int queueid, int client, int locked);
int snd_seq_queue_set_locked(int queueid, int client, int locked);
int snd_seq_queue_timer_open(int queueid);
int snd_seq_queue_timer_close(int queueid);
int snd_seq_queue_use(int queueid, int client, int use);
int snd_seq_queue_is_used(int queueid, int client);

int snd_seq_control_queue(struct snd_seq_event *ev, int atomic, int hop);

#if defined(i386) || defined(i486)

#define udiv_qrnnd(q, r, n1, n0, d) \
  __asm__ ("divl %4"		\
	   : "=a" ((u32)(q)),	\
	     "=d" ((u32)(r))	\
	   : "0" ((u32)(n0)),	\
	     "1" ((u32)(n1)),	\
	     "rm" ((u32)(d)))

#define u64_div(x,y,q) do {u32 __tmp; udiv_qrnnd(q, __tmp, (x)>>32, x, y);} while (0)
#define u64_mod(x,y,r) do {u32 __tmp; udiv_qrnnd(__tmp, q, (x)>>32, x, y);} while (0)
#define u64_divmod(x,y,q,r) udiv_qrnnd(q, r, (x)>>32, x, y)

#else
#define u64_div(x,y,q)	((q) = (u32)((u64)(x) / (u64)(y)))
#define u64_mod(x,y,r)	((r) = (u32)((u64)(x) % (u64)(y)))
#define u64_divmod(x,y,q,r) (u64_div(x,y,q), u64_mod(x,y,r))
#endif


#endif
