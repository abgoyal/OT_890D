obj-y = aio.o elf_aux.o execvp.o file.o helper.o irq.o main.o mem.o process.o \
	registers.o sigio.o signal.o start_up.o time.o tty.o uaccess.o \
	umid.o tls.o user_syms.o util.o drivers/ sys-$(SUBARCH)/ skas/

USER_OBJS := $(user-objs-y) aio.o elf_aux.o execvp.o file.o helper.o irq.o \
	main.o mem.o process.o registers.o sigio.o signal.o start_up.o time.o \
	tty.o tls.o uaccess.o umid.o util.o

CFLAGS_user_syms.o += -DSUBARCH_$(SUBARCH)

HAVE_AIO_ABI := $(shell [ -r /usr/include/linux/aio_abi.h ] && \
	echo -DHAVE_AIO_ABI )
CFLAGS_aio.o += $(HAVE_AIO_ABI)

include arch/um/scripts/Makefile.rules
