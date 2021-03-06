
#ifndef __S390_UACCESS_H
#define __S390_UACCESS_H

#include <linux/sched.h>
#include <linux/errno.h>

#define VERIFY_READ     0
#define VERIFY_WRITE    1



#define MAKE_MM_SEG(a)  ((mm_segment_t) { (a) })


#define KERNEL_DS       MAKE_MM_SEG(0)
#define USER_DS         MAKE_MM_SEG(1)

#define get_ds()        (KERNEL_DS)
#define get_fs()        (current->thread.mm_segment)

#define set_fs(x) \
({									\
	unsigned long __pto;						\
	current->thread.mm_segment = (x);				\
	__pto = current->thread.mm_segment.ar4 ?			\
		S390_lowcore.user_asce : S390_lowcore.kernel_asce;	\
	__ctl_load(__pto, 7, 7);					\
})

#define segment_eq(a,b) ((a).ar4 == (b).ar4)


static inline int __access_ok(const void __user *addr, unsigned long size)
{
	return 1;
}
#define access_ok(type,addr,size) __access_ok(addr,size)


struct exception_table_entry
{
        unsigned long insn, fixup;
};

struct uaccess_ops {
	size_t (*copy_from_user)(size_t, const void __user *, void *);
	size_t (*copy_from_user_small)(size_t, const void __user *, void *);
	size_t (*copy_to_user)(size_t, void __user *, const void *);
	size_t (*copy_to_user_small)(size_t, void __user *, const void *);
	size_t (*copy_in_user)(size_t, void __user *, const void __user *);
	size_t (*clear_user)(size_t, void __user *);
	size_t (*strnlen_user)(size_t, const char __user *);
	size_t (*strncpy_from_user)(size_t, const char __user *, char *);
	int (*futex_atomic_op)(int op, int __user *, int oparg, int *old);
	int (*futex_atomic_cmpxchg)(int __user *, int old, int new);
};

extern struct uaccess_ops uaccess;
extern struct uaccess_ops uaccess_std;
extern struct uaccess_ops uaccess_mvcos;
extern struct uaccess_ops uaccess_mvcos_switch;
extern struct uaccess_ops uaccess_pt;

static inline int __put_user_fn(size_t size, void __user *ptr, void *x)
{
	size = uaccess.copy_to_user_small(size, ptr, x);
	return size ? -EFAULT : size;
}

static inline int __get_user_fn(size_t size, const void __user *ptr, void *x)
{
	size = uaccess.copy_from_user_small(size, ptr, x);
	return size ? -EFAULT : size;
}

#define __put_user(x, ptr) \
({								\
	__typeof__(*(ptr)) __x = (x);				\
	int __pu_err = -EFAULT;					\
        __chk_user_ptr(ptr);                                    \
	switch (sizeof (*(ptr))) {				\
	case 1:							\
	case 2:							\
	case 4:							\
	case 8:							\
		__pu_err = __put_user_fn(sizeof (*(ptr)),	\
					 ptr, &__x);		\
		break;						\
	default:						\
		__put_user_bad();				\
		break;						\
	 }							\
	__pu_err;						\
})

#define put_user(x, ptr)					\
({								\
	might_sleep();						\
	__put_user(x, ptr);					\
})


extern int __put_user_bad(void) __attribute__((noreturn));

#define __get_user(x, ptr)					\
({								\
	int __gu_err = -EFAULT;					\
	__chk_user_ptr(ptr);					\
	switch (sizeof(*(ptr))) {				\
	case 1: {						\
		unsigned char __x;				\
		__gu_err = __get_user_fn(sizeof (*(ptr)),	\
					 ptr, &__x);		\
		(x) = *(__force __typeof__(*(ptr)) *) &__x;	\
		break;						\
	};							\
	case 2: {						\
		unsigned short __x;				\
		__gu_err = __get_user_fn(sizeof (*(ptr)),	\
					 ptr, &__x);		\
		(x) = *(__force __typeof__(*(ptr)) *) &__x;	\
		break;						\
	};							\
	case 4: {						\
		unsigned int __x;				\
		__gu_err = __get_user_fn(sizeof (*(ptr)),	\
					 ptr, &__x);		\
		(x) = *(__force __typeof__(*(ptr)) *) &__x;	\
		break;						\
	};							\
	case 8: {						\
		unsigned long long __x;				\
		__gu_err = __get_user_fn(sizeof (*(ptr)),	\
					 ptr, &__x);		\
		(x) = *(__force __typeof__(*(ptr)) *) &__x;	\
		break;						\
	};							\
	default:						\
		__get_user_bad();				\
		break;						\
	}							\
	__gu_err;						\
})

#define get_user(x, ptr)					\
({								\
	might_sleep();						\
	__get_user(x, ptr);					\
})

extern int __get_user_bad(void) __attribute__((noreturn));

#define __put_user_unaligned __put_user
#define __get_user_unaligned __get_user

static inline unsigned long __must_check
__copy_to_user(void __user *to, const void *from, unsigned long n)
{
	if (__builtin_constant_p(n) && (n <= 256))
		return uaccess.copy_to_user_small(n, to, from);
	else
		return uaccess.copy_to_user(n, to, from);
}

#define __copy_to_user_inatomic __copy_to_user
#define __copy_from_user_inatomic __copy_from_user

static inline unsigned long __must_check
copy_to_user(void __user *to, const void *from, unsigned long n)
{
	might_sleep();
	if (access_ok(VERIFY_WRITE, to, n))
		n = __copy_to_user(to, from, n);
	return n;
}

static inline unsigned long __must_check
__copy_from_user(void *to, const void __user *from, unsigned long n)
{
	if (__builtin_constant_p(n) && (n <= 256))
		return uaccess.copy_from_user_small(n, from, to);
	else
		return uaccess.copy_from_user(n, from, to);
}

static inline unsigned long __must_check
copy_from_user(void *to, const void __user *from, unsigned long n)
{
	might_sleep();
	if (access_ok(VERIFY_READ, from, n))
		n = __copy_from_user(to, from, n);
	else
		memset(to, 0, n);
	return n;
}

static inline unsigned long __must_check
__copy_in_user(void __user *to, const void __user *from, unsigned long n)
{
	return uaccess.copy_in_user(n, to, from);
}

static inline unsigned long __must_check
copy_in_user(void __user *to, const void __user *from, unsigned long n)
{
	might_sleep();
	if (__access_ok(from,n) && __access_ok(to,n))
		n = __copy_in_user(to, from, n);
	return n;
}

static inline long __must_check
strncpy_from_user(char *dst, const char __user *src, long count)
{
        long res = -EFAULT;
        might_sleep();
        if (access_ok(VERIFY_READ, src, 1))
		res = uaccess.strncpy_from_user(count, src, dst);
        return res;
}

static inline unsigned long
strnlen_user(const char __user * src, unsigned long n)
{
	might_sleep();
	return uaccess.strnlen_user(n, src);
}

#define strlen_user(str) strnlen_user(str, ~0UL)


static inline unsigned long __must_check
__clear_user(void __user *to, unsigned long n)
{
	return uaccess.clear_user(n, to);
}

static inline unsigned long __must_check
clear_user(void __user *to, unsigned long n)
{
	might_sleep();
	if (access_ok(VERIFY_WRITE, to, n))
		n = uaccess.clear_user(n, to);
	return n;
}

#endif /* __S390_UACCESS_H */
