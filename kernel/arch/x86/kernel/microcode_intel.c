
#include <linux/capability.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/sched.h>
#include <linux/smp_lock.h>
#include <linux/cpumask.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>
#include <linux/miscdevice.h>
#include <linux/spinlock.h>
#include <linux/mm.h>
#include <linux/fs.h>
#include <linux/mutex.h>
#include <linux/cpu.h>
#include <linux/firmware.h>
#include <linux/platform_device.h>

#include <asm/msr.h>
#include <asm/uaccess.h>
#include <asm/processor.h>
#include <asm/microcode.h>

MODULE_DESCRIPTION("Microcode Update Driver");
MODULE_AUTHOR("Tigran Aivazian <tigran@aivazian.fsnet.co.uk>");
MODULE_LICENSE("GPL");

struct microcode_header_intel {
	unsigned int            hdrver;
	unsigned int            rev;
	unsigned int            date;
	unsigned int            sig;
	unsigned int            cksum;
	unsigned int            ldrver;
	unsigned int            pf;
	unsigned int            datasize;
	unsigned int            totalsize;
	unsigned int            reserved[3];
};

struct microcode_intel {
	struct microcode_header_intel hdr;
	unsigned int            bits[0];
};

/* microcode format is extended from prescott processors */
struct extended_signature {
	unsigned int            sig;
	unsigned int            pf;
	unsigned int            cksum;
};

struct extended_sigtable {
	unsigned int            count;
	unsigned int            cksum;
	unsigned int            reserved[3];
	struct extended_signature sigs[0];
};

#define DEFAULT_UCODE_DATASIZE 	(2000)
#define MC_HEADER_SIZE		(sizeof(struct microcode_header_intel))
#define DEFAULT_UCODE_TOTALSIZE (DEFAULT_UCODE_DATASIZE + MC_HEADER_SIZE)
#define EXT_HEADER_SIZE		(sizeof(struct extended_sigtable))
#define EXT_SIGNATURE_SIZE	(sizeof(struct extended_signature))
#define DWSIZE			(sizeof(u32))
#define get_totalsize(mc) \
	(((struct microcode_intel *)mc)->hdr.totalsize ? \
	 ((struct microcode_intel *)mc)->hdr.totalsize : \
	 DEFAULT_UCODE_TOTALSIZE)

#define get_datasize(mc) \
	(((struct microcode_intel *)mc)->hdr.datasize ? \
	 ((struct microcode_intel *)mc)->hdr.datasize : DEFAULT_UCODE_DATASIZE)

#define sigmatch(s1, s2, p1, p2) \
	(((s1) == (s2)) && (((p1) & (p2)) || (((p1) == 0) && ((p2) == 0))))

#define exttable_size(et) ((et)->count * EXT_SIGNATURE_SIZE + EXT_HEADER_SIZE)

/* serialize access to the physical write to MSR 0x79 */
static DEFINE_SPINLOCK(microcode_update_lock);

static int collect_cpu_info(int cpu_num, struct cpu_signature *csig)
{
	struct cpuinfo_x86 *c = &cpu_data(cpu_num);
	unsigned long flags;
	unsigned int val[2];

	memset(csig, 0, sizeof(*csig));

	if (c->x86_vendor != X86_VENDOR_INTEL || c->x86 < 6 ||
	    cpu_has(c, X86_FEATURE_IA64)) {
		printk(KERN_ERR "microcode: CPU%d not a capable Intel "
			"processor\n", cpu_num);
		return -1;
	}

	csig->sig = cpuid_eax(0x00000001);

	if ((c->x86_model >= 5) || (c->x86 > 6)) {
		/* get processor flags from MSR 0x17 */
		rdmsr(MSR_IA32_PLATFORM_ID, val[0], val[1]);
		csig->pf = 1 << ((val[1] >> 18) & 7);
	}

	/* serialize access to the physical write to MSR 0x79 */
	spin_lock_irqsave(&microcode_update_lock, flags);

	wrmsr(MSR_IA32_UCODE_REV, 0, 0);
	/* see notes above for revision 1.07.  Apparent chip bug */
	sync_core();
	/* get the current revision from MSR 0x8B */
	rdmsr(MSR_IA32_UCODE_REV, val[0], csig->rev);
	spin_unlock_irqrestore(&microcode_update_lock, flags);

	pr_debug("microcode: collect_cpu_info : sig=0x%x, pf=0x%x, rev=0x%x\n",
			csig->sig, csig->pf, csig->rev);

	return 0;
}

static inline int update_match_cpu(struct cpu_signature *csig, int sig, int pf)
{
	return (!sigmatch(sig, csig->sig, pf, csig->pf)) ? 0 : 1;
}

static inline int 
update_match_revision(struct microcode_header_intel *mc_header,	int rev)
{
	return (mc_header->rev <= rev) ? 0 : 1;
}

static int microcode_sanity_check(void *mc)
{
	struct microcode_header_intel *mc_header = mc;
	struct extended_sigtable *ext_header = NULL;
	struct extended_signature *ext_sig;
	unsigned long total_size, data_size, ext_table_size;
	int sum, orig_sum, ext_sigcount = 0, i;

	total_size = get_totalsize(mc_header);
	data_size = get_datasize(mc_header);
	if (data_size + MC_HEADER_SIZE > total_size) {
		printk(KERN_ERR "microcode: error! "
			"Bad data size in microcode data file\n");
		return -EINVAL;
	}

	if (mc_header->ldrver != 1 || mc_header->hdrver != 1) {
		printk(KERN_ERR "microcode: error! "
			"Unknown microcode update format\n");
		return -EINVAL;
	}
	ext_table_size = total_size - (MC_HEADER_SIZE + data_size);
	if (ext_table_size) {
		if ((ext_table_size < EXT_HEADER_SIZE)
		 || ((ext_table_size - EXT_HEADER_SIZE) % EXT_SIGNATURE_SIZE)) {
			printk(KERN_ERR "microcode: error! "
				"Small exttable size in microcode data file\n");
			return -EINVAL;
		}
		ext_header = mc + MC_HEADER_SIZE + data_size;
		if (ext_table_size != exttable_size(ext_header)) {
			printk(KERN_ERR "microcode: error! "
				"Bad exttable size in microcode data file\n");
			return -EFAULT;
		}
		ext_sigcount = ext_header->count;
	}

	/* check extended table checksum */
	if (ext_table_size) {
		int ext_table_sum = 0;
		int *ext_tablep = (int *)ext_header;

		i = ext_table_size / DWSIZE;
		while (i--)
			ext_table_sum += ext_tablep[i];
		if (ext_table_sum) {
			printk(KERN_WARNING "microcode: aborting, "
				"bad extended signature table checksum\n");
			return -EINVAL;
		}
	}

	/* calculate the checksum */
	orig_sum = 0;
	i = (MC_HEADER_SIZE + data_size) / DWSIZE;
	while (i--)
		orig_sum += ((int *)mc)[i];
	if (orig_sum) {
		printk(KERN_ERR "microcode: aborting, bad checksum\n");
		return -EINVAL;
	}
	if (!ext_table_size)
		return 0;
	/* check extended signature checksum */
	for (i = 0; i < ext_sigcount; i++) {
		ext_sig = (void *)ext_header + EXT_HEADER_SIZE +
			  EXT_SIGNATURE_SIZE * i;
		sum = orig_sum
			- (mc_header->sig + mc_header->pf + mc_header->cksum)
			+ (ext_sig->sig + ext_sig->pf + ext_sig->cksum);
		if (sum) {
			printk(KERN_ERR "microcode: aborting, bad checksum\n");
			return -EINVAL;
		}
	}
	return 0;
}

static int
get_matching_microcode(struct cpu_signature *cpu_sig, void *mc, int rev)
{
	struct microcode_header_intel *mc_header = mc;
	struct extended_sigtable *ext_header;
	unsigned long total_size = get_totalsize(mc_header);
	int ext_sigcount, i;
	struct extended_signature *ext_sig;

	if (!update_match_revision(mc_header, rev))
		return 0;

	if (update_match_cpu(cpu_sig, mc_header->sig, mc_header->pf))
		return 1;

	/* Look for ext. headers: */
	if (total_size <= get_datasize(mc_header) + MC_HEADER_SIZE)
		return 0;

	ext_header = mc + get_datasize(mc_header) + MC_HEADER_SIZE;
	ext_sigcount = ext_header->count;
	ext_sig = (void *)ext_header + EXT_HEADER_SIZE;

	for (i = 0; i < ext_sigcount; i++) {
		if (update_match_cpu(cpu_sig, ext_sig->sig, ext_sig->pf))
			return 1;
		ext_sig++;
	}
	return 0;
}

static void apply_microcode(int cpu)
{
	unsigned long flags;
	unsigned int val[2];
	int cpu_num = raw_smp_processor_id();
	struct ucode_cpu_info *uci = ucode_cpu_info + cpu;
	struct microcode_intel *mc_intel = uci->mc;

	/* We should bind the task to the CPU */
	BUG_ON(cpu_num != cpu);

	if (mc_intel == NULL)
		return;

	/* serialize access to the physical write to MSR 0x79 */
	spin_lock_irqsave(&microcode_update_lock, flags);

	/* write microcode via MSR 0x79 */
	wrmsr(MSR_IA32_UCODE_WRITE,
	      (unsigned long) mc_intel->bits,
	      (unsigned long) mc_intel->bits >> 16 >> 16);
	wrmsr(MSR_IA32_UCODE_REV, 0, 0);

	/* see notes above for revision 1.07.  Apparent chip bug */
	sync_core();

	/* get the current revision from MSR 0x8B */
	rdmsr(MSR_IA32_UCODE_REV, val[0], val[1]);

	spin_unlock_irqrestore(&microcode_update_lock, flags);
	if (val[1] != mc_intel->hdr.rev) {
		printk(KERN_ERR "microcode: CPU%d update from revision "
			"0x%x to 0x%x failed\n", cpu_num, uci->cpu_sig.rev, val[1]);
		return;
	}
	printk(KERN_INFO "microcode: CPU%d updated from revision "
	       "0x%x to 0x%x, date = %04x-%02x-%02x \n",
		cpu_num, uci->cpu_sig.rev, val[1],
		mc_intel->hdr.date & 0xffff,
		mc_intel->hdr.date >> 24,
		(mc_intel->hdr.date >> 16) & 0xff);
	uci->cpu_sig.rev = val[1];
}

static int generic_load_microcode(int cpu, void *data, size_t size,
		int (*get_ucode_data)(void *, const void *, size_t))
{
	struct ucode_cpu_info *uci = ucode_cpu_info + cpu;
	u8 *ucode_ptr = data, *new_mc = NULL, *mc;
	int new_rev = uci->cpu_sig.rev;
	unsigned int leftover = size;

	while (leftover) {
		struct microcode_header_intel mc_header;
		unsigned int mc_size;

		if (get_ucode_data(&mc_header, ucode_ptr, sizeof(mc_header)))
			break;

		mc_size = get_totalsize(&mc_header);
		if (!mc_size || mc_size > leftover) {
			printk(KERN_ERR "microcode: error!"
					"Bad data in microcode data file\n");
			break;
		}

		mc = vmalloc(mc_size);
		if (!mc)
			break;

		if (get_ucode_data(mc, ucode_ptr, mc_size) ||
		    microcode_sanity_check(mc) < 0) {
			vfree(mc);
			break;
		}

		if (get_matching_microcode(&uci->cpu_sig, mc, new_rev)) {
			if (new_mc)
				vfree(new_mc);
			new_rev = mc_header.rev;
			new_mc  = mc;
		} else
			vfree(mc);

		ucode_ptr += mc_size;
		leftover  -= mc_size;
	}

	if (new_mc) {
		if (!leftover) {
			if (uci->mc)
				vfree(uci->mc);
			uci->mc = (struct microcode_intel *)new_mc;
			pr_debug("microcode: CPU%d found a matching microcode update with"
				 " version 0x%x (current=0x%x)\n",
				cpu, new_rev, uci->cpu_sig.rev);
		} else
			vfree(new_mc);
	}

	return (int)leftover;
}

static int get_ucode_fw(void *to, const void *from, size_t n)
{
	memcpy(to, from, n);
	return 0;
}

static int request_microcode_fw(int cpu, struct device *device)
{
	char name[30];
	struct cpuinfo_x86 *c = &cpu_data(cpu);
	const struct firmware *firmware;
	int ret;

	/* We should bind the task to the CPU */
	BUG_ON(cpu != raw_smp_processor_id());
	sprintf(name, "intel-ucode/%02x-%02x-%02x",
		c->x86, c->x86_model, c->x86_mask);
	ret = request_firmware(&firmware, name, device);
	if (ret) {
		pr_debug("microcode: data file %s load failed\n", name);
		return ret;
	}

	ret = generic_load_microcode(cpu, (void*)firmware->data, firmware->size,
			&get_ucode_fw);

	release_firmware(firmware);

	return ret;
}

static int get_ucode_user(void *to, const void *from, size_t n)
{
	return copy_from_user(to, from, n);
}

static int request_microcode_user(int cpu, const void __user *buf, size_t size)
{
	/* We should bind the task to the CPU */
	BUG_ON(cpu != raw_smp_processor_id());

	return generic_load_microcode(cpu, (void*)buf, size, &get_ucode_user);
}

static void microcode_fini_cpu(int cpu)
{
	struct ucode_cpu_info *uci = ucode_cpu_info + cpu;

	vfree(uci->mc);
	uci->mc = NULL;
}

static struct microcode_ops microcode_intel_ops = {
	.request_microcode_user		  = request_microcode_user,
	.request_microcode_fw             = request_microcode_fw,
	.collect_cpu_info                 = collect_cpu_info,
	.apply_microcode                  = apply_microcode,
	.microcode_fini_cpu               = microcode_fini_cpu,
};

struct microcode_ops * __init init_intel_microcode(void)
{
	return &microcode_intel_ops;
}

