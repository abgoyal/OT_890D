

#include <linux/platform_device.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/rfkill.h>
#include <linux/delay.h>

#include <mach/mt_bt.h>

extern void mt_wifi_power_on(void);
extern void mt_wifi_power_off(void);
extern int mt_wifi_suspend(pm_message_t state);
extern int mt_wifi_resume(pm_message_t state);

#if defined(CONFIG_WIMAX_MT71X8)
extern void mt7118_wimax_power_on(void);
extern void mt7118_wimax_power_off(void);
extern int mt7118_wimax_suspend(pm_message_t state);
extern int mt7118_wimax_resume(pm_message_t state);
#endif

void rfkill_switch_all(enum rfkill_type type, enum rfkill_state state);

//#define RFKILL_PM_TEST

enum {
    RFDEV_POWER_OFF,
    RFDEV_POWER_ON,
    RFDEV_SUSPEND
};

struct mt_rfdev {
    const char *name;
    struct rfkill *rfkill;
    spinlock_t lock;
    int  state;
    int  (*suspend)(pm_message_t state);
    int  (*resume)(pm_message_t state);
    void (*power_on)(void);
    void (*power_off)(void);
};

static struct mt_rfdev bt = {
    .name = "BT",
    .state = RFDEV_POWER_OFF,
    .lock = SPIN_LOCK_UNLOCKED,
    .suspend = mt_bt_suspend,
    .resume = mt_bt_resume,
    .power_on = mt_bt_power_on,
    .power_off = mt_bt_power_off
};

static struct mt_rfdev wlan = {
    .name = "WLAN",
    .state = RFDEV_POWER_OFF,
    .lock = SPIN_LOCK_UNLOCKED,
    .suspend = mt_wifi_suspend,
    .resume = mt_wifi_resume,
    .power_on = mt_wifi_power_on,
    .power_off = mt_wifi_power_off
};

#if defined(CONFIG_WIMAX_MT71X8)
static struct mt_rfdev wimax = {
    .name = "WIMAX",
    .state = RFDEV_POWER_OFF,
    .lock = SPIN_LOCK_UNLOCKED,
    .suspend = mt7118_wimax_suspend,
    .resume = mt7118_wimax_resume,
    .power_on = mt7118_wimax_power_on,
    .power_off = mt7118_wimax_power_off
};
#endif

static int mt_rfkill_power(struct mt_rfdev *dev, int on)
{
    BUG_ON(dev->state == RFDEV_SUSPEND);

    printk(KERN_INFO "[%s] Power %s\n", dev->name, on ? "on" : "off");

    spin_lock(&dev->lock);
    dev->state = on ? RFDEV_POWER_ON : RFDEV_POWER_OFF;
    spin_unlock(&dev->lock);

    if (on) {
        dev->power_on();
    } else {
        dev->power_off();
    }
    
    return 0;
}

static int mt_rfkill_toggle_radio(void *data, enum rfkill_state state)
{
    struct mt_rfdev *dev = (struct mt_rfdev *)data;

    printk(KERN_INFO "[%s] rfkill_set_power(state %d)\n", dev->name, state);

    switch (state) 
    {
    case RFKILL_STATE_UNBLOCKED:
        mt_rfkill_power(dev, 1);
        break;
    case RFKILL_STATE_SOFT_BLOCKED:
        mt_rfkill_power(dev, 0);
        break;
    default:
        printk(KERN_ERR "[%s] bad state %d\n", dev->name, state);
    }
    return 0;
}

#ifdef CONFIG_PM
static int mt_rfkill_suspend_dev(struct mt_rfdev *dev, pm_message_t state)
{   
    printk(KERN_INFO "[%s] PM Suspend\n", dev->name);

    spin_lock(&dev->lock);

    if (dev->state != RFDEV_POWER_ON)
        goto err;

    if (dev->suspend && dev->suspend(state) == 0) {
        dev->state = RFDEV_SUSPEND;
        spin_unlock(&dev->lock);
        return rfkill_force_state(dev->rfkill, RFKILL_STATE_SOFT_BLOCKED);
    }
err:
    spin_unlock(&dev->lock);
    
    return -1;
}

static int mt_rfkill_resume_dev(struct mt_rfdev *dev, pm_message_t state)
{
    printk(KERN_INFO "[%s] PM Resume\n", dev->name);

    spin_lock(&dev->lock);

    if (dev->state != RFDEV_SUSPEND)
        goto err;

    if (dev->resume && dev->resume(state) == 0) {
        dev->state = RFDEV_POWER_ON;
        spin_unlock(&dev->lock);
        return rfkill_force_state(dev->rfkill, RFKILL_STATE_UNBLOCKED);    
    }
err:
    spin_unlock(&dev->lock);
    
    return -1;
}

static int mt_rfkill_suspend(struct platform_device *pdev, pm_message_t state)
{
    printk(KERN_INFO "[RFKill] mt_rfkill_suspend\n");

    if (state.event == PM_EVENT_SUSPEND) {        
#if defined(CONFIG_WIMAX_MT71X8)
        (void)mt_rfkill_suspend_dev(&wimax, state);
#endif
    }
    return 0;
}
static int mt_rfkill_resume(struct platform_device *pdev)
{
    pm_message_t state;

    printk(KERN_INFO "[RFKill] mt_rfkill_resume\n");
    state.event = PM_EVENT_RESUME;
#if defined(CONFIG_WIMAX_MT71X8)
    (void)mt_rfkill_resume_dev(&wimax, state);
#endif
    return 0;
}
#endif

#ifdef RFKILL_PM_TEST
static ssize_t mt_rfkill_pm_test(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
    struct rfkill *rfk = container_of(dev, struct rfkill, dev);
    struct mt_rfdev *rfdev = (struct mt_rfdev *)rfk->data;

    if (rfk->type == RFKILL_TYPE_WLAN) {
        pm_message_t s;
        
        printk(KERN_INFO "==== [WIFI] Power on ====\n");
        mt_rfkill_power(rfdev, 1);
        msleep(10);
        printk(KERN_INFO "==== [WIFI] Suspend ====\n");
        s.event = PM_EVENT_SUSPEND;
        mt_rfkill_suspend_dev(rfdev, s);
        msleep(10);
        printk(KERN_INFO "==== [WIFI] Resume ====\n");
        s.event = PM_EVENT_RESUME;
        mt_rfkill_resume_dev(rfdev, s);
        msleep(10);
        printk(KERN_INFO "==== [WIFI] Power off ====\n");
        mt_rfkill_power(rfdev, 0);
        msleep(10);
        printk(KERN_INFO "==== [WIFI] Suspend ====\n");
        s.event = PM_EVENT_SUSPEND;
        mt_rfkill_suspend_dev(rfdev, s);
        msleep(10);
        printk(KERN_INFO "==== [WIFI] Resume ====\n");
        s.event = PM_EVENT_RESUME;
        mt_rfkill_resume_dev(rfdev, s);
        msleep(10);
        printk(KERN_INFO "==== [WIFI] Power on ====\n");
        mt_rfkill_power(rfdev, 1);
        msleep(10);            
        printk(KERN_INFO "==== [WIFI] Resume ====\n");
        s.event = PM_EVENT_RESUME;
        mt_rfkill_resume_dev(rfdev, s);
        msleep(10);
        printk(KERN_INFO "==== [WIFI] Power off ====\n");
        mt_rfkill_power(rfdev, 0);
        msleep(10);
        printk(KERN_INFO "==== [WIFI] Resume ====\n");
        s.event = PM_EVENT_RESUME;
        mt_rfkill_resume_dev(rfdev, s);
        msleep(10);
        printk(KERN_INFO "==== [ BT ] Power on ====\n");
        mt_rfkill_power(&bt, 1);
        msleep(10);
        printk(KERN_INFO "==== [WIFI] Power on ====\n");
        mt_rfkill_power(rfdev, 1);
        msleep(10);
        printk(KERN_INFO "==== [ BT ] Power off ====\n");
        mt_rfkill_power(&bt, 0);
        msleep(10);
    }
	return count;
}

DEVICE_ATTR(pmtst, S_IWUSR | S_IRUGO, NULL, mt_rfkill_pm_test);
#endif

static int __init mt_rfkill_probe(struct platform_device *pdev)
{
    int rc = 0;
    struct rfkill *rfk;

    /* default to bluetooth off */
    mt_rfkill_power(&bt, 0);  
    rfkill_set_default(RFKILL_TYPE_BLUETOOTH, RFKILL_STATE_SOFT_BLOCKED);

    rfk = rfkill_allocate(&pdev->dev, RFKILL_TYPE_BLUETOOTH);
    if (!rfk)
        return -ENOMEM;

    rfk->name  = "mt6611";
    rfk->state = RFKILL_STATE_SOFT_BLOCKED;
    /* userspace cannot take exclusive control */
    rfk->user_claim_unsupported = 1;
    rfk->user_claim = 0;
    rfk->data = &bt;  // user data
    rfk->toggle_radio = mt_rfkill_toggle_radio;

    rc = rfkill_register(rfk);
 
    if (rc == 0) {
        bt.rfkill = rfk;
        #ifdef RFKILL_PM_TEST
        rc = device_create_file(&rfk->dev, &dev_attr_pmtst);
        #endif
    } else {
        rfkill_free(rfk);
    }      

    /* default to wlan off */
    mt_rfkill_power(&wlan, 0);
    rfkill_set_default(RFKILL_TYPE_WLAN, RFKILL_STATE_SOFT_BLOCKED);

    rfk = rfkill_allocate(&pdev->dev, RFKILL_TYPE_WLAN);
    if (!rfk)
        return -ENOMEM;

    rfk->name   = "mt5921";
    rfk->state  = RFKILL_STATE_SOFT_BLOCKED;
    /* userspace cannot take exclusive control */
    rfk->user_claim_unsupported = 1;
    rfk->user_claim = 0;
    rfk->data = &wlan;  // user data
    rfk->toggle_radio = mt_rfkill_toggle_radio;

    rc = rfkill_register(rfk);
 
    if (rc == 0) {
        wlan.rfkill = rfk;
        #ifdef RFKILL_PM_TEST
        rc = device_create_file(&rfk->dev, &dev_attr_pmtst);
        #endif
    } else {
        rfkill_free(rfk);
    }
    
#if defined(CONFIG_WIMAX_MT71X8)
    /* default to wimax off */
    mt_rfkill_power(&wimax, 0);
    rfkill_set_default(RFKILL_TYPE_WIMAX, RFKILL_STATE_SOFT_BLOCKED);

    rfk = rfkill_allocate(&pdev->dev, RFKILL_TYPE_WIMAX);
    if (!rfk)
        return -ENOMEM;

    rfk->name   = "mt7118";
    rfk->state  = RFKILL_STATE_SOFT_BLOCKED;
    /* userspace cannot take exclusive control */
    rfk->user_claim_unsupported = 1;
    rfk->user_claim = 0;
    rfk->data = &wimax;  // user data
    rfk->toggle_radio = mt_rfkill_toggle_radio;

    rc = rfkill_register(rfk);
 
    if (rc == 0) {        
        wimax.rfkill = rfk;
        #ifdef RFKILL_PM_TEST
        rc = device_create_file(&rfk->dev, &dev_attr_pmtst);
        #endif
    } else {        
        rfkill_free(rfk);
    }
#endif
    
    return rc;
}

static struct platform_driver mt_rfkill_driver = 
{
    .probe    = mt_rfkill_probe,
#ifdef CONFIG_PM
    .suspend  = mt_rfkill_suspend,
    .resume   = mt_rfkill_resume,
#endif
    .driver = 
    {
        .name = "mt-rfkill",
        .owner = THIS_MODULE,
    },
};

static int __init mt_rfkill_init(void)
{
    return platform_driver_register(&mt_rfkill_driver);
}

module_init(mt_rfkill_init);
MODULE_DESCRIPTION("mt-rfkill");
MODULE_AUTHOR("JinKwan <jk.huang@mediatek.com>");
MODULE_LICENSE("GPL");

