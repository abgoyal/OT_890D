
#include <linux/moduleloader.h>
#include <linux/elf.h>
#include <linux/vmalloc.h>
#include <linux/fs.h>
#include <linux/string.h>
#include <linux/kernel.h>
#include <linux/bug.h>

#if 0
#define DEBUGP printk
#else
#define DEBUGP(fmt, ...)
#endif

void *module_alloc(unsigned long size)
{
	if (size == 0)
		return NULL;
	return vmalloc_exec(size);
}

void module_free(struct module *mod, void *module_region)
{
	vfree(module_region);
	/* FIXME: If module_region == mod->init_region, trim exception
	 * table entries. */
}

int module_frob_arch_sections(Elf_Ehdr *hdr,
			      Elf_Shdr *sechdrs,
			      char *secstrings,
			      struct module *mod)
{
	return 0;
}

static void reloc_put16(uint8_t *p, uint32_t val)
{
	p[0] = val & 0xff;
	p[1] = (val >> 8) & 0xff;
}

static void reloc_put24(uint8_t *p, uint32_t val)
{
	reloc_put16(p, val);
	p[2] = (val >> 16) & 0xff;
}

static void reloc_put32(uint8_t *p, uint32_t val)
{
	reloc_put16(p, val);
	reloc_put16(p+2, val >> 16);
}

int apply_relocate(Elf32_Shdr *sechdrs,
		   const char *strtab,
		   unsigned int symindex,
		   unsigned int relsec,
		   struct module *me)
{
	printk(KERN_ERR "module %s: RELOCATION unsupported\n",
	       me->name);
	return -ENOEXEC;
}

int apply_relocate_add(Elf32_Shdr *sechdrs,
		       const char *strtab,
		       unsigned int symindex,
		       unsigned int relsec,
		       struct module *me)
{
	unsigned int i;
	Elf32_Rela *rel = (void *)sechdrs[relsec].sh_addr;
	Elf32_Sym *sym;
	Elf32_Addr relocation;
	uint8_t *location;
	uint32_t value;

	DEBUGP("Applying relocate section %u to %u\n",
	       relsec, sechdrs[relsec].sh_info);

	for (i = 0; i < sechdrs[relsec].sh_size / sizeof(*rel); i++) {
		/* this is where to make the change */
		location = (void *)sechdrs[sechdrs[relsec].sh_info].sh_addr
			+ rel[i].r_offset;

		/* this is the symbol the relocation is referring to (note that
		 * all undefined symbols have been resolved by the caller) */
		sym = (Elf32_Sym *)sechdrs[symindex].sh_addr
			+ ELF32_R_SYM(rel[i].r_info);

		/* this is the adjustment to be made */
		relocation = sym->st_value + rel[i].r_addend;

		switch (ELF32_R_TYPE(rel[i].r_info)) {
			/* for the first four relocation types, we simply
			 * store the adjustment at the location given */
		case R_MN10300_32:
			reloc_put32(location, relocation);
			break;
		case R_MN10300_24:
			reloc_put24(location, relocation);
			break;
		case R_MN10300_16:
			reloc_put16(location, relocation);
			break;
		case R_MN10300_8:
			*location = relocation;
			break;

			/* for the next three relocation types, we write the
			 * adjustment with the address subtracted over the
			 * value at the location given */
		case R_MN10300_PCREL32:
			value = relocation - (uint32_t) location;
			reloc_put32(location, value);
			break;
		case R_MN10300_PCREL16:
			value = relocation - (uint32_t) location;
			reloc_put16(location, value);
			break;
		case R_MN10300_PCREL8:
			*location = relocation - (uint32_t) location;
			break;

		default:
			printk(KERN_ERR "module %s: Unknown relocation: %u\n",
			       me->name, ELF32_R_TYPE(rel[i].r_info));
			return -ENOEXEC;
		}
	}
	return 0;
}

int module_finalize(const Elf_Ehdr *hdr,
		    const Elf_Shdr *sechdrs,
		    struct module *me)
{
	return module_bug_finalize(hdr, sechdrs, me);
}

void module_arch_cleanup(struct module *mod)
{
	module_bug_cleanup(mod);
}
