


#ifndef _ME4600_DI_H_
#define _ME4600_DI_H_

#include "mesubdevice.h"

#ifdef __KERNEL__

typedef struct me4600_di_subdevice {
	/* Inheritance */
	me_subdevice_t base;			/**< The subdevice base class. */

	/* Attributes */
	spinlock_t subdevice_lock;		/**< Spin lock to protect the subdevice from concurrent access. */
	spinlock_t *ctrl_reg_lock;		/**< Spin lock to protect #ctrl_reg from concurrent access. */

	unsigned long port_reg;			/**< Register holding the port status. */
	unsigned long ctrl_reg;			/**< Register to configure the port direction. */
#ifdef MEDEBUG_DEBUG_REG
	unsigned long reg_base;
#endif
} me4600_di_subdevice_t;

me4600_di_subdevice_t *me4600_di_constructor(uint32_t reg_base,
					     spinlock_t * ctrl_reg_lock);

#endif
#endif
