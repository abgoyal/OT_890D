obj-$(CONFIG_FPE_NWFPE)		+= nwfpe.o

nwfpe-y				+= fpa11.o fpa11_cpdo.o fpa11_cpdt.o \
				   fpa11_cprt.o fpmodule.o fpopcode.o \
				   softfloat.o single_cpdo.o double_cpdo.o

nwfpe-$(CONFIG_FPE_NWFPE_XP)	+= extended_cpdo.o
nwfpe-$(CONFIG_CPU_32)		+= entry.o
