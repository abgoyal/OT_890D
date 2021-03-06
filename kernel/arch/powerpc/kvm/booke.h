

#ifndef __KVM_BOOKE_H__
#define __KVM_BOOKE_H__

#include <linux/types.h>
#include <linux/kvm_host.h>
#include "timing.h"

/* interrupt priortity ordering */
#define BOOKE_IRQPRIO_DATA_STORAGE 0
#define BOOKE_IRQPRIO_INST_STORAGE 1
#define BOOKE_IRQPRIO_ALIGNMENT 2
#define BOOKE_IRQPRIO_PROGRAM 3
#define BOOKE_IRQPRIO_FP_UNAVAIL 4
#define BOOKE_IRQPRIO_SYSCALL 5
#define BOOKE_IRQPRIO_AP_UNAVAIL 6
#define BOOKE_IRQPRIO_DTLB_MISS 7
#define BOOKE_IRQPRIO_ITLB_MISS 8
#define BOOKE_IRQPRIO_MACHINE_CHECK 9
#define BOOKE_IRQPRIO_DEBUG 10
#define BOOKE_IRQPRIO_CRITICAL 11
#define BOOKE_IRQPRIO_WATCHDOG 12
#define BOOKE_IRQPRIO_EXTERNAL 13
#define BOOKE_IRQPRIO_FIT 14
#define BOOKE_IRQPRIO_DECREMENTER 15

static inline void kvmppc_set_msr(struct kvm_vcpu *vcpu, u32 new_msr)
{
	if ((new_msr & MSR_PR) != (vcpu->arch.msr & MSR_PR))
		kvmppc_mmu_priv_switch(vcpu, new_msr & MSR_PR);

	vcpu->arch.msr = new_msr;

	if (vcpu->arch.msr & MSR_WE) {
		kvm_vcpu_block(vcpu);
		kvmppc_set_exit_type(vcpu, EMULATED_MTMSRWE_EXITS);
	};
}

#endif /* __KVM_BOOKE_H__ */
