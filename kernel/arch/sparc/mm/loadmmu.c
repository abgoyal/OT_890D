

#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/init.h>

#include <asm/system.h>
#include <asm/page.h>
#include <asm/pgtable.h>
#include <asm/mmu_context.h>
#include <asm/oplib.h>

struct ctx_list *ctx_list_pool;
struct ctx_list ctx_free;
struct ctx_list ctx_used;

extern void ld_mmu_sun4c(void);
extern void ld_mmu_srmmu(void);

void __init load_mmu(void)
{
	switch(sparc_cpu_model) {
	case sun4c:
	case sun4:
		ld_mmu_sun4c();
		break;
	case sun4m:
	case sun4d:
		ld_mmu_srmmu();
		break;
	default:
		prom_printf("load_mmu: %d unsupported\n", (int)sparc_cpu_model);
		prom_halt();
	}
	btfixup();
}
