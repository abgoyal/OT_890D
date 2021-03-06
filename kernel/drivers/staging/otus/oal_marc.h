
/*  Module Name : oal_marc.h                                            */
/*                                                                      */
/*  Abstract                                                            */
/*      This module contains warpper definitions.                       */
/*                                                                      */
/*  NOTES                                                               */
/*      Platform dependent.                                             */
/*                                                                      */
/************************************************************************/

#ifndef _OAL_MARC_H
#define _OAL_MARC_H

#include "oal_dt.h"
#include "usbdrv.h"

#define ZM_OS_LINUX_FUNC

/***** Critical section *****/
/* Declare for critical section */
#ifndef ZM_HALPLUS_LOCK
#define zmw_get_wlan_dev(dev)    struct zsWlanDev *wd = (struct zsWlanDev*) ((((struct usbdrv_private*)dev->priv)->wd))

#define zmw_declare_for_critical_section() unsigned long irqFlag;

/* Enter critical section */
#define zmw_enter_critical_section(dev) \
        spin_lock_irqsave(&(((struct usbdrv_private *)(dev->priv))->cs_lock), irqFlag);

/* leave critical section */
#define zmw_leave_critical_section(dev) \
        spin_unlock_irqrestore(&(((struct usbdrv_private *)(dev->priv))->cs_lock), irqFlag);
#else
#define zmw_get_wlan_dev(dev)    struct zsWlanDev *wd = zfwGetWlanDev(dev);

/* Declare for critical section */
#define zmw_declare_for_critical_section()

/* Enter critical section */
#define zmw_enter_critical_section(dev) \
        zfwEnterCriticalSection(dev);

/* leave critical section */
#define zmw_leave_critical_section(dev) \
        zfwLeaveCriticalSection(dev);
#endif

/***** Byte order converting *****/
#ifdef ZM_CONFIG_BIG_ENDIAN
#define zmw_cpu_to_le32(v)    (((v & 0xff000000) >> 24) | \
                               ((v & 0x00ff0000) >> 8)  | \
                               ((v & 0x0000ff00) << 8)  | \
                               ((v & 0x000000ff) << 24))

#define zmw_le32_to_cpu(v)    (((v & 0xff000000) >> 24) | \
                               ((v & 0x00ff0000) >> 8)  | \
                               ((v & 0x0000ff00) << 8)  | \
                               ((v & 0x000000ff) << 24))

#define zmw_cpu_to_le16(v)    (((v & 0xff00) >> 8) | \
                               ((v & 0x00ff) << 8))

#define zmw_le16_to_cpu(v)    (((v & 0xff00) >> 8) | \
                               ((v & 0x00ff) << 8))
#else
#define zmw_cpu_to_le32(v)    (v)
#define zmw_le32_to_cpu(v)    (v)
#define zmw_cpu_to_le16(v)    (v)
#define zmw_le16_to_cpu(v)    (v)
#endif

/***** Buffer access *****/
/* Called to read/write buffer */
#ifndef ZM_HALPLUS_LOCK

#define zmw_buf_readb(dev, buf, offset) *(u8_t*)((u8_t*)buf->data+offset)
#define zmw_buf_readh(dev, buf, offset) zmw_cpu_to_le16(*(u16_t*)((u8_t*)buf->data+offset))
#define zmw_buf_writeb(dev, buf, offset, value) *(u8_t*)((u8_t*)buf->data+offset) = value
#define zmw_buf_writeh(dev, buf, offset, value) *(u16_t*)((u8_t*)buf->data+offset) = zmw_cpu_to_le16(value)
#define zmw_buf_get_buffer(dev, buf) (u8_t*)(buf->data)

#else

#define zmw_buf_readb(dev, buf, offset) zfwBufReadByte(dev, buf, offset)
#define zmw_buf_readh(dev, buf, offset) zfwBufReadHalfWord(dev, buf, offset)
#define zmw_buf_writeb(dev, buf, offset, value) zfwBufWriteByte(dev, buf, offset, value)
#define zmw_buf_writeh(dev, buf, offset, value) zfwBufWriteHalfWord(dev, buf, offset, value)
#define zmw_buf_get_buffer(dev, buf) zfwGetBuffer(dev, buf)

#endif

/***** Debug message *****/
#if 0
#define zm_debug_msg0(msg) printk("%s:%s\n", __func__, msg);
#define zm_debug_msg1(msg, val) printk("%s:%s%ld\n", __func__, \
        msg, (u32_t)val);
#define zm_debug_msg2(msg, val) printk("%s:%s%lxh\n", __func__, \
        msg, (u32_t)val);
#define zm_debug_msg_s(msg, val) printk("%s:%s%s\n", __func__, \
        msg, val);
#define zm_debug_msg_p(msg, val1, val2) printk("%s:%s%01ld.%02ld\n", __func__, \
        msg, (val1/val2), (((val1*100)/val2)%100));
#define zm_dbg(S) printk S
#else
#define zm_debug_msg0(msg)
#define zm_debug_msg1(msg, val)
#define zm_debug_msg2(msg, val)
#define zm_debug_msg_s(msg, val)
#define zm_debug_msg_p(msg, val1, val2)
#define zm_dbg(S)
#endif

#define zm_assert(expr) if(!(expr)) {                           \
        printk( "Atheors Assertion failed! %s,%s,%s,line=%d\n",   \
        #expr,__FILE__,__func__,__LINE__);                  \
        }

#define DbgPrint printk

#endif /* #ifndef _OAL_MARC_H */
