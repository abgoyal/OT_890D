

#ifndef _RATECTRL_H
#define _RATECTRL_H

#define ZM_RATE_CTRL_PROBING_INTERVAL_MS    1000 //1000ms
#define ZM_RATE_CTRL_MIN_PROBING_PACKET     8

#define ZM_MIN_RATE_FAIL_COUNT              20

#define ZM_RATE_PROBING_THRESHOLD           15  //6%
#define ZM_RATE_SUCCESS_PROBING             10

#define ZM_RATE_CTRL_RSSI_VARIATION         5  //TBD

extern const u32_t zcRateToPhyCtrl[];

extern void zfRateCtrlInitCell(zdev_t* dev, struct zsRcCell* rcCell, u8_t type, u8_t gBand, u8_t SG40);
extern u16_t zfRateCtrlGetTxRate(zdev_t* dev, struct zsRcCell* rcCell, u16_t* probing);
extern void zfRateCtrlTxFailEvent(zdev_t* dev, struct zsRcCell* rcCell, u8_t aggRate, u32_t retryRate);
extern void zfRateCtrlTxSuccessEvent(zdev_t* dev, struct zsRcCell* rcCell, u8_t successRate);
extern void zfRateCtrlAggrSta(zdev_t* dev);
#endif
