
#ifndef _ASM_BOOTINFO_H
#define _ASM_BOOTINFO_H

#include <linux/types.h>
#include <asm/setup.h>


#define  MACH_UNKNOWN		0	/* whatever...			*/

#define  MACH_DSUNKNOWN		0
#define  MACH_DS23100		1	/* DECstation 2100 or 3100	*/
#define  MACH_DS5100		2	/* DECsystem 5100		*/
#define  MACH_DS5000_200	3	/* DECstation 5000/200		*/
#define  MACH_DS5000_1XX	4	/* DECstation 5000/120, 125, 133, 150 */
#define  MACH_DS5000_XX		5	/* DECstation 5000/20, 25, 33, 50 */
#define  MACH_DS5000_2X0	6	/* DECstation 5000/240, 260	*/
#define  MACH_DS5400		7	/* DECsystem 5400		*/
#define  MACH_DS5500		8	/* DECsystem 5500		*/
#define  MACH_DS5800		9	/* DECsystem 5800		*/
#define  MACH_DS5900		10	/* DECsystem 5900		*/

#define MACH_MSP4200_EVAL       0	/* PMC-Sierra MSP4200 Evaluation */
#define MACH_MSP4200_GW         1	/* PMC-Sierra MSP4200 Gateway demo */
#define MACH_MSP4200_FPGA       2	/* PMC-Sierra MSP4200 Emulation */
#define MACH_MSP7120_EVAL       3	/* PMC-Sierra MSP7120 Evaluation */
#define MACH_MSP7120_GW         4	/* PMC-Sierra MSP7120 Residential GW */
#define MACH_MSP7120_FPGA       5	/* PMC-Sierra MSP7120 Emulation */
#define MACH_MSP_OTHER        255	/* PMC-Sierra unknown board type */

#define	MACH_MIKROTIK_RB532	0	/* Mikrotik RouterBoard 532 	*/
#define MACH_MIKROTIK_RB532A	1	/* Mikrotik RouterBoard 532A 	*/

#define CL_SIZE			COMMAND_LINE_SIZE

extern char *system_type;
const char *get_system_type(void);

extern unsigned long mips_machtype;

#define BOOT_MEM_MAP_MAX	32
#define BOOT_MEM_RAM		1
#define BOOT_MEM_ROM_DATA	2
#define BOOT_MEM_RESERVED	3

struct boot_mem_map {
	int nr_map;
	struct boot_mem_map_entry {
		phys_t addr;	/* start of memory segment */
		phys_t size;	/* size of memory segment */
		long type;		/* type of memory segment */
	} map[BOOT_MEM_MAP_MAX];
};

extern struct boot_mem_map boot_mem_map;

extern void add_memory_region(phys_t start, phys_t size, long type);

extern void prom_init(void);
extern void prom_free_prom_memory(void);

extern void free_init_pages(const char *what,
			    unsigned long begin, unsigned long end);

extern char arcs_cmdline[CL_SIZE];

extern unsigned long fw_arg0, fw_arg1, fw_arg2, fw_arg3;

extern void plat_mem_setup(void);

#endif /* _ASM_BOOTINFO_H */
