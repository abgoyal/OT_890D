

#include <linux/suspend.h>

struct pxa_cpu_pm_fns {
	int	save_count;
	void	(*save)(unsigned long *);
	void	(*restore)(unsigned long *);
	int	(*valid)(suspend_state_t state);
	void	(*enter)(suspend_state_t state);
	int	(*prepare)(void);
	void	(*finish)(void);
};

extern struct pxa_cpu_pm_fns *pxa_cpu_pm_fns;

/* sleep.S */
extern void pxa25x_cpu_suspend(unsigned int);
extern void pxa27x_cpu_suspend(unsigned int);
extern void pxa_cpu_resume(void);

extern int pxa_pm_enter(suspend_state_t state);
