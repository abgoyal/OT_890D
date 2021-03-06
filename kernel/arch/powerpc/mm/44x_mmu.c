

#include <linux/init.h>
#include <asm/mmu.h>
#include <asm/system.h>
#include <asm/page.h>
#include <asm/cacheflush.h>

#include "mmu_decl.h"

unsigned int tlb_44x_index; /* = 0 */
unsigned int tlb_44x_hwater = PPC44x_TLB_SIZE - 1 - PPC44x_EARLY_TLBS;
int icache_44x_need_flush;

static void __init ppc44x_update_tlb_hwater(void)
{
	extern unsigned int tlb_44x_patch_hwater_D[];
	extern unsigned int tlb_44x_patch_hwater_I[];

	/* The TLB miss handlers hard codes the watermark in a cmpli
	 * instruction to improve performances rather than loading it
	 * from the global variable. Thus, we patch the instructions
	 * in the 2 TLB miss handlers when updating the value
	 */
	tlb_44x_patch_hwater_D[0] = (tlb_44x_patch_hwater_D[0] & 0xffff0000) |
		tlb_44x_hwater;
	flush_icache_range((unsigned long)&tlb_44x_patch_hwater_D[0],
			   (unsigned long)&tlb_44x_patch_hwater_D[1]);
	tlb_44x_patch_hwater_I[0] = (tlb_44x_patch_hwater_I[0] & 0xffff0000) |
		tlb_44x_hwater;
	flush_icache_range((unsigned long)&tlb_44x_patch_hwater_I[0],
			   (unsigned long)&tlb_44x_patch_hwater_I[1]);
}

static void __init ppc44x_pin_tlb(unsigned int virt, unsigned int phys)
{
	unsigned int entry = tlb_44x_hwater--;

	ppc44x_update_tlb_hwater();

	__asm__ __volatile__(
		"tlbwe	%2,%3,%4\n"
		"tlbwe	%1,%3,%5\n"
		"tlbwe	%0,%3,%6\n"
	:
	: "r" (PPC44x_TLB_SW | PPC44x_TLB_SR | PPC44x_TLB_SX | PPC44x_TLB_G),
	  "r" (phys),
	  "r" (virt | PPC44x_TLB_VALID | PPC44x_TLB_256M),
	  "r" (entry),
	  "i" (PPC44x_TLB_PAGEID),
	  "i" (PPC44x_TLB_XLAT),
	  "i" (PPC44x_TLB_ATTRIB));
}

void __init MMU_init_hw(void)
{
	ppc44x_update_tlb_hwater();

	flush_instruction_cache();
}

unsigned long __init mmu_mapin_ram(void)
{
	unsigned long addr;

	/* Pin in enough TLBs to cover any lowmem not covered by the
	 * initial 256M mapping established in head_44x.S */
	for (addr = PPC_PIN_SIZE; addr < lowmem_end_addr;
	     addr += PPC_PIN_SIZE)
		ppc44x_pin_tlb(addr + PAGE_OFFSET, addr);

	return total_lowmem;
}
