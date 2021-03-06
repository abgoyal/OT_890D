

#include "r8180.h"

#define RTL8225_ANAPARAM_ON  0xa0000b59
#define RTL8225_ANAPARAM_OFF 0xa00beb59
#define RTL8225_ANAPARAM2_OFF 0x840dec11
#define RTL8225_ANAPARAM2_ON  0x860dec11
#define RTL8225_ANAPARAM_SLEEP 0xa00bab59
#define RTL8225_ANAPARAM2_SLEEP 0x840dec11

#ifdef CONFIG_RTL8185B
void rtl8225z2_rf_init(struct net_device *dev);
void rtl8225z2_rf_set_chan(struct net_device *dev,short ch);
void rtl8225z2_rf_close(struct net_device *dev);

void rtl8225_host_pci_init(struct net_device *dev);
void rtl8225_host_usb_init(struct net_device *dev);

void write_rtl8225(struct net_device *dev, u8 adr, u16 data);
void RF_WriteReg(struct net_device *dev, u8 offset, u32	data);
u32 RF_ReadReg(struct net_device *dev, u8 offset);
#endif
void rtl8225_rf_init(struct net_device *dev);
void rtl8225_rf_set_chan(struct net_device *dev,short ch);
void rtl8225_rf_close(struct net_device *dev);
void rtl8225_rf_sleep(struct net_device *dev);
void rtl8225_rf_wakeup(struct net_device *dev);
void rtl8180_set_mode(struct net_device *dev,int mode);
void rtl8180_set_mode(struct net_device *dev,int mode);
bool SetZebraRFPowerState8185(struct net_device *dev,RT_RF_POWER_STATE  eRFPowerState);
void rtl8225z4_rf_sleep(struct net_device *dev);
void rtl8225z4_rf_wakeup(struct net_device *dev);

