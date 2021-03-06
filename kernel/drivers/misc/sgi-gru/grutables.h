

#ifndef __GRUTABLES_H__
#define __GRUTABLES_H__


#include <linux/rmap.h>
#include <linux/interrupt.h>
#include <linux/mutex.h>
#include <linux/wait.h>
#include <linux/mmu_notifier.h>
#include "gru.h"
#include "gruhandles.h"

extern struct gru_stats_s gru_stats;
extern struct gru_blade_state *gru_base[];
extern unsigned long gru_start_paddr, gru_end_paddr;

#define GRU_MAX_BLADES		MAX_NUMNODES
#define GRU_MAX_GRUS		(GRU_MAX_BLADES * GRU_CHIPLETS_PER_BLADE)

#define GRU_DRIVER_ID_STR	"SGI GRU Device Driver"
#define GRU_DRIVER_VERSION_STR	"0.80"

struct gru_stats_s {
	atomic_long_t vdata_alloc;
	atomic_long_t vdata_free;
	atomic_long_t gts_alloc;
	atomic_long_t gts_free;
	atomic_long_t vdata_double_alloc;
	atomic_long_t gts_double_allocate;
	atomic_long_t assign_context;
	atomic_long_t assign_context_failed;
	atomic_long_t free_context;
	atomic_long_t load_context;
	atomic_long_t unload_context;
	atomic_long_t steal_context;
	atomic_long_t steal_context_failed;
	atomic_long_t nopfn;
	atomic_long_t break_cow;
	atomic_long_t asid_new;
	atomic_long_t asid_next;
	atomic_long_t asid_wrap;
	atomic_long_t asid_reuse;
	atomic_long_t intr;
	atomic_long_t call_os;
	atomic_long_t call_os_check_for_bug;
	atomic_long_t call_os_wait_queue;
	atomic_long_t user_flush_tlb;
	atomic_long_t user_unload_context;
	atomic_long_t user_exception;
	atomic_long_t set_task_slice;
	atomic_long_t migrate_check;
	atomic_long_t migrated_retarget;
	atomic_long_t migrated_unload;
	atomic_long_t migrated_unload_delay;
	atomic_long_t migrated_nopfn_retarget;
	atomic_long_t migrated_nopfn_unload;
	atomic_long_t tlb_dropin;
	atomic_long_t tlb_dropin_fail_no_asid;
	atomic_long_t tlb_dropin_fail_upm;
	atomic_long_t tlb_dropin_fail_invalid;
	atomic_long_t tlb_dropin_fail_range_active;
	atomic_long_t tlb_dropin_fail_idle;
	atomic_long_t tlb_dropin_fail_fmm;
	atomic_long_t mmu_invalidate_range;
	atomic_long_t mmu_invalidate_page;
	atomic_long_t mmu_clear_flush_young;
	atomic_long_t flush_tlb;
	atomic_long_t flush_tlb_gru;
	atomic_long_t flush_tlb_gru_tgh;
	atomic_long_t flush_tlb_gru_zero_asid;

	atomic_long_t copy_gpa;

	atomic_long_t mesq_receive;
	atomic_long_t mesq_receive_none;
	atomic_long_t mesq_send;
	atomic_long_t mesq_send_failed;
	atomic_long_t mesq_noop;
	atomic_long_t mesq_send_unexpected_error;
	atomic_long_t mesq_send_lb_overflow;
	atomic_long_t mesq_send_qlimit_reached;
	atomic_long_t mesq_send_amo_nacked;
	atomic_long_t mesq_send_put_nacked;
	atomic_long_t mesq_qf_not_full;
	atomic_long_t mesq_qf_locked;
	atomic_long_t mesq_qf_noop_not_full;
	atomic_long_t mesq_qf_switch_head_failed;
	atomic_long_t mesq_qf_unexpected_error;
	atomic_long_t mesq_noop_unexpected_error;
	atomic_long_t mesq_noop_lb_overflow;
	atomic_long_t mesq_noop_qlimit_reached;
	atomic_long_t mesq_noop_amo_nacked;
	atomic_long_t mesq_noop_put_nacked;

};

#define OPT_DPRINT	1
#define OPT_STATS	2
#define GRU_QUICKLOOK	4


#define IRQ_GRU			110	/* Starting IRQ number for interrupts */

/* Delay in jiffies between attempts to assign a GRU context */
#define GRU_ASSIGN_DELAY	((HZ * 20) / 1000)

#define GRU_STEAL_DELAY		((HZ * 200) / 1000)

#define STAT(id)	do {						\
				if (gru_options & OPT_STATS)		\
					atomic_long_inc(&gru_stats.id);	\
			} while (0)

#ifdef CONFIG_SGI_GRU_DEBUG
#define gru_dbg(dev, fmt, x...)						\
	do {								\
		if (gru_options & OPT_DPRINT)				\
			dev_dbg(dev, "%s: " fmt, __func__, x);		\
	} while (0)
#else
#define gru_dbg(x...)
#endif

#define MAX_ASID	0xfffff0
#define MIN_ASID	8
#define ASID_INC	8	/* number of regions */

/* Generate a GRU asid value from a GRU base asid & a virtual address. */
#if defined CONFIG_IA64
#define VADDR_HI_BIT		64
#define GRUREGION(addr)		((addr) >> (VADDR_HI_BIT - 3) & 3)
#elif defined CONFIG_X86_64
#define VADDR_HI_BIT		48
#define GRUREGION(addr)		(0)		/* ZZZ could do better */
#else
#error "Unsupported architecture"
#endif
#define GRUASID(asid, addr)	((asid) + GRUREGION(addr))


struct gru_state;

struct gru_mm_tracker {
	unsigned int		mt_asid_gen;	/* ASID wrap count */
	int			mt_asid;	/* current base ASID for gru */
	unsigned short		mt_ctxbitmap;	/* bitmap of contexts using
						   asid */
};

struct gru_mm_struct {
	struct mmu_notifier	ms_notifier;
	atomic_t		ms_refcnt;
	spinlock_t		ms_asid_lock;	/* protects ASID assignment */
	atomic_t		ms_range_active;/* num range_invals active */
	char			ms_released;
	wait_queue_head_t	ms_wait_queue;
	DECLARE_BITMAP(ms_asidmap, GRU_MAX_GRUS);
	struct gru_mm_tracker	ms_asids[GRU_MAX_GRUS];
};

struct gru_vma_data {
	spinlock_t		vd_lock;	/* Serialize access to vma */
	struct list_head	vd_head;	/* head of linked list of gts */
	long			vd_user_options;/* misc user option flags */
	int			vd_cbr_au_count;
	int			vd_dsr_au_count;
};

struct gru_thread_state {
	struct list_head	ts_next;	/* list - head at vma-private */
	struct mutex		ts_ctxlock;	/* load/unload CTX lock */
	struct mm_struct	*ts_mm;		/* mm currently mapped to
						   context */
	struct vm_area_struct	*ts_vma;	/* vma of GRU context */
	struct gru_state	*ts_gru;	/* GRU where the context is
						   loaded */
	struct gru_mm_struct	*ts_gms;	/* asid & ioproc struct */
	unsigned long		ts_cbr_map;	/* map of allocated CBRs */
	unsigned long		ts_dsr_map;	/* map of allocated DATA
						   resources */
	unsigned long		ts_steal_jiffies;/* jiffies when context last
						    stolen */
	long			ts_user_options;/* misc user option flags */
	pid_t			ts_tgid_owner;	/* task that is using the
						   context - for migration */
	int			ts_tsid;	/* thread that owns the
						   structure */
	int			ts_tlb_int_select;/* target cpu if interrupts
						     enabled */
	int			ts_ctxnum;	/* context number where the
						   context is loaded */
	atomic_t		ts_refcnt;	/* reference count GTS */
	unsigned char		ts_dsr_au_count;/* Number of DSR resources
						   required for contest */
	unsigned char		ts_cbr_au_count;/* Number of CBR resources
						   required for contest */
	char			ts_force_unload;/* force context to be unloaded
						   after migration */
	char			ts_cbr_idx[GRU_CBR_AU];/* CBR numbers of each
							  allocated CB */
	unsigned long		ts_gdata[0];	/* save area for GRU data (CB,
						   DS, CBE) */
};

#define TSID(a, v)		(((a) - (v)->vm_start) / GRU_GSEG_PAGESIZE)
#define UGRUADDR(gts)		((gts)->ts_vma->vm_start +		\
					(gts)->ts_tsid * GRU_GSEG_PAGESIZE)

#define NULLCTX			(-1)	/* if context not loaded into GRU */


struct gru_state {
	struct gru_blade_state	*gs_blade;		/* GRU state for entire
							   blade */
	unsigned long		gs_gru_base_paddr;	/* Physical address of
							   gru segments (64) */
	void			*gs_gru_base_vaddr;	/* Virtual address of
							   gru segments (64) */
	unsigned char		gs_gid;			/* unique GRU number */
	unsigned char		gs_tgh_local_shift;	/* used to pick TGH for
							   local flush */
	unsigned char		gs_tgh_first_remote;	/* starting TGH# for
							   remote flush */
	unsigned short		gs_blade_id;		/* blade of GRU */
	spinlock_t		gs_asid_lock;		/* lock used for
							   assigning asids */
	spinlock_t		gs_lock;		/* lock used for
							   assigning contexts */

	/* -- the following are protected by the gs_asid_lock spinlock ---- */
	unsigned int		gs_asid;		/* Next availe ASID */
	unsigned int		gs_asid_limit;		/* Limit of available
							   ASIDs */
	unsigned int		gs_asid_gen;		/* asid generation.
							   Inc on wrap */

	/* --- the following fields are protected by the gs_lock spinlock --- */
	unsigned long		gs_context_map;		/* bitmap to manage
							   contexts in use */
	unsigned long		gs_cbr_map;		/* bitmap to manage CB
							   resources */
	unsigned long		gs_dsr_map;		/* bitmap used to manage
							   DATA resources */
	unsigned int		gs_reserved_cbrs;	/* Number of kernel-
							   reserved cbrs */
	unsigned int		gs_reserved_dsr_bytes;	/* Bytes of kernel-
							   reserved dsrs */
	unsigned short		gs_active_contexts;	/* number of contexts
							   in use */
	struct gru_thread_state	*gs_gts[GRU_NUM_CCH];	/* GTS currently using
							   the context */
};

struct gru_blade_state {
	void			*kernel_cb;		/* First kernel
							   reserved cb */
	void			*kernel_dsr;		/* First kernel
							   reserved DSR */
	/* ---- the following are protected by the bs_lock spinlock ---- */
	spinlock_t		bs_lock;		/* lock used for
							   stealing contexts */
	int			bs_lru_ctxnum;		/* STEAL - last context
							   stolen */
	struct gru_state	*bs_lru_gru;		/* STEAL - last gru
							   stolen */

	struct gru_state	bs_grus[GRU_CHIPLETS_PER_BLADE];
};

#define get_tfm_for_cpu(g, c)						\
	((struct gru_tlb_fault_map *)get_tfm((g)->gs_gru_base_vaddr, (c)))
#define get_tfh_by_index(g, i)						\
	((struct gru_tlb_fault_handle *)get_tfh((g)->gs_gru_base_vaddr, (i)))
#define get_tgh_by_index(g, i)						\
	((struct gru_tlb_global_handle *)get_tgh((g)->gs_gru_base_vaddr, (i)))
#define get_cbe_by_index(g, i)						\
	((struct gru_control_block_extended *)get_cbe((g)->gs_gru_base_vaddr,\
			(i)))


/* Given a blade# & chiplet#, get a pointer to the GRU */
#define get_gru(b, c)		(&gru_base[b]->bs_grus[c])

/* Number of bytes to save/restore when unloading/loading GRU contexts */
#define DSR_BYTES(dsr)		((dsr) * GRU_DSR_AU_BYTES)
#define CBR_BYTES(cbr)		((cbr) * GRU_HANDLE_BYTES * GRU_CBR_AU_SIZE * 2)

/* Convert a user CB number to the actual CBRNUM */
#define thread_cbr_number(gts, n) ((gts)->ts_cbr_idx[(n) / GRU_CBR_AU_SIZE] \
				  * GRU_CBR_AU_SIZE + (n) % GRU_CBR_AU_SIZE)

/* Convert a gid to a pointer to the GRU */
#define GID_TO_GRU(gid)							\
	(gru_base[(gid) / GRU_CHIPLETS_PER_BLADE] ?			\
		(&gru_base[(gid) / GRU_CHIPLETS_PER_BLADE]->		\
			bs_grus[(gid) % GRU_CHIPLETS_PER_BLADE]) :	\
	 NULL)

/* Scan all active GRUs in a GRU bitmap */
#define for_each_gru_in_bitmap(gid, map)				\
	for ((gid) = find_first_bit((map), GRU_MAX_GRUS); (gid) < GRU_MAX_GRUS;\
		(gid)++, (gid) = find_next_bit((map), GRU_MAX_GRUS, (gid)))

/* Scan all active GRUs on a specific blade */
#define for_each_gru_on_blade(gru, nid, i)				\
	for ((gru) = gru_base[nid]->bs_grus, (i) = 0;			\
			(i) < GRU_CHIPLETS_PER_BLADE;			\
			(i)++, (gru)++)

/* Scan all active GTSs on a gru. Note: must hold ss_lock to use this macro. */
#define for_each_gts_on_gru(gts, gru, ctxnum)				\
	for ((ctxnum) = 0; (ctxnum) < GRU_NUM_CCH; (ctxnum)++)		\
		if (((gts) = (gru)->gs_gts[ctxnum]))

/* Scan each CBR whose bit is set in a TFM (or copy of) */
#define for_each_cbr_in_tfm(i, map)					\
	for ((i) = find_first_bit(map, GRU_NUM_CBE);			\
			(i) < GRU_NUM_CBE;				\
			(i)++, (i) = find_next_bit(map, GRU_NUM_CBE, i))

/* Scan each CBR in a CBR bitmap. Note: multiple CBRs in an allocation unit */
#define for_each_cbr_in_allocation_map(i, map, k)			\
	for ((k) = find_first_bit(map, GRU_CBR_AU); (k) < GRU_CBR_AU;	\
			(k) = find_next_bit(map, GRU_CBR_AU, (k) + 1)) 	\
		for ((i) = (k)*GRU_CBR_AU_SIZE;				\
				(i) < ((k) + 1) * GRU_CBR_AU_SIZE; (i)++)

/* Scan each DSR in a DSR bitmap. Note: multiple DSRs in an allocation unit */
#define for_each_dsr_in_allocation_map(i, map, k)			\
	for ((k) = find_first_bit((const unsigned long *)map, GRU_DSR_AU);\
			(k) < GRU_DSR_AU;				\
			(k) = find_next_bit((const unsigned long *)map,	\
					  GRU_DSR_AU, (k) + 1))		\
		for ((i) = (k) * GRU_DSR_AU_CL;				\
				(i) < ((k) + 1) * GRU_DSR_AU_CL; (i)++)

#define gseg_physical_address(gru, ctxnum)				\
		((gru)->gs_gru_base_paddr + ctxnum * GRU_GSEG_STRIDE)
#define gseg_virtual_address(gru, ctxnum)				\
		((gru)->gs_gru_base_vaddr + ctxnum * GRU_GSEG_STRIDE)


/* Lock hierarchy checking enabled only in emulator */

static inline void __lock_handle(void *h)
{
	while (test_and_set_bit(1, h))
		cpu_relax();
}

static inline void __unlock_handle(void *h)
{
	clear_bit(1, h);
}

static inline void lock_cch_handle(struct gru_context_configuration_handle *cch)
{
	__lock_handle(cch);
}

static inline void unlock_cch_handle(struct gru_context_configuration_handle
				     *cch)
{
	__unlock_handle(cch);
}

static inline void lock_tgh_handle(struct gru_tlb_global_handle *tgh)
{
	__lock_handle(tgh);
}

static inline void unlock_tgh_handle(struct gru_tlb_global_handle *tgh)
{
	__unlock_handle(tgh);
}

struct gru_unload_context_req;

extern struct vm_operations_struct gru_vm_ops;
extern struct device *grudev;

extern struct gru_vma_data *gru_alloc_vma_data(struct vm_area_struct *vma,
				int tsid);
extern struct gru_thread_state *gru_find_thread_state(struct vm_area_struct
				*vma, int tsid);
extern struct gru_thread_state *gru_alloc_thread_state(struct vm_area_struct
				*vma, int tsid);
extern void gru_unload_context(struct gru_thread_state *gts, int savestate);
extern void gts_drop(struct gru_thread_state *gts);
extern void gru_tgh_flush_init(struct gru_state *gru);
extern int gru_kservices_init(struct gru_state *gru);
extern irqreturn_t gru_intr(int irq, void *dev_id);
extern int gru_handle_user_call_os(unsigned long address);
extern int gru_user_flush_tlb(unsigned long arg);
extern int gru_user_unload_context(unsigned long arg);
extern int gru_get_exception_detail(unsigned long arg);
extern int gru_set_task_slice(long address);
extern int gru_cpu_fault_map_id(void);
extern struct vm_area_struct *gru_find_vma(unsigned long vaddr);
extern void gru_flush_all_tlb(struct gru_state *gru);
extern int gru_proc_init(void);
extern void gru_proc_exit(void);

extern unsigned long gru_reserve_cb_resources(struct gru_state *gru,
		int cbr_au_count, char *cbmap);
extern unsigned long gru_reserve_ds_resources(struct gru_state *gru,
		int dsr_au_count, char *dsmap);
extern int gru_fault(struct vm_area_struct *, struct vm_fault *vmf);
extern struct gru_mm_struct *gru_register_mmu_notifier(void);
extern void gru_drop_mmu_notifier(struct gru_mm_struct *gms);

extern void gru_flush_tlb_range(struct gru_mm_struct *gms, unsigned long start,
					unsigned long len);

extern unsigned long gru_options;

#endif /* __GRUTABLES_H__ */
