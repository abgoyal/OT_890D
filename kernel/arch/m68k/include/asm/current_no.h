
#ifndef _M68KNOMMU_CURRENT_H
#define _M68KNOMMU_CURRENT_H

#include <linux/thread_info.h>

struct task_struct;

static inline struct task_struct *get_current(void)
{
	return(current_thread_info()->task);
}

#define	current	get_current()

#endif /* _M68KNOMMU_CURRENT_H */
