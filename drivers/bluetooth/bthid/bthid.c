/*
 *  bthid.c
 *
 * Copyright (C) 2010 Broadcom Corporation
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */
#include <linux/poll.h>
#include <linux/miscdevice.h>
#include <linux/hid.h>


MODULE_AUTHOR("Daniel McDowell <mcdowell@broadcom.com>");
MODULE_DESCRIPTION("User level driver support for Bluetooth HID input");
MODULE_SUPPORTED_DEVICE("bthid");
MODULE_LICENSE("GPL");


#define BTHID_NAME              "bthid"
#define BTHID_MINOR             224
#define BTHID_IOCTL_RPT_DSCP    1
#define BTHID_IOCTL_RPT_DI      3
#define BTHID_IOCTL_RPT_NAME    5
#define BTHID_MAX_CTRL_BUF_LEN  508
#define BTHID_DEV_NAME_LEN 128 /* size of hid_device's name field  */


struct bthid_ctrl {
    int   size;
    char  buf[BTHID_MAX_CTRL_BUF_LEN];
};

struct bthid_device {
    struct input_dev   *dev;
    struct hid_device  *hid;
    int                dscp_set;
};

struct bthid_di {
    u16 vendor_id;      /* vendor ID */
    u16 product_id;     /* product ID */
    u16 version;        /* version */
    u8 ctry_code;      /*Country Code.*/
};

static int bthid_ll_start(struct hid_device *hid)
{
    printk("######## bthid_ll_start: hid = %p ########\n", hid);
    return 0;
}

static void bthid_ll_stop(struct hid_device *hid)
{
    printk("######## bthid_ll_stop: hid = %p ########\n", hid);
}

static int bthid_ll_open(struct hid_device *hid)
{
    printk("######## bthid_ll_open: hid = %p ########\n", hid);
    return 0;
}

static void bthid_ll_close(struct hid_device *hid)
{
    printk("######## bthid_ll_close: hid = %p ########\n", hid);
}

static int bthid_ll_hidinput_event(struct input_dev *dev, unsigned int type, 
                                   unsigned int code, int value)
{
    /*
    printk("######## bthid_ll_hidinput_event: dev = %p, type = %d, code = %d, value = %d ########\n",
           dev, type, code, value);
    */
    return 0;
}

static int bthid_ll_parse(struct hid_device *hid)
{
    int ret;
    unsigned char *buf;
    struct bthid_ctrl *p_ctrl = hid->driver_data;

    printk("######## bthid_ll_parse: hid = %p ########\n", hid);
    
    buf = kmalloc(p_ctrl->size, GFP_KERNEL);
    if (!buf)
    {
        return -ENOMEM;
    }

    memcpy(buf, p_ctrl->buf, p_ctrl->size);

    ret = hid_parse_report(hid, buf, p_ctrl->size);
    kfree(buf);

    printk("######## bthid_ll_parse: status = %d, ret = %d ########\n", hid->status, ret);

    return ret;
}

static struct hid_ll_driver bthid_ll_driver = {
    .start                = bthid_ll_start,
    .stop                 = bthid_ll_stop,
    .open                 = bthid_ll_open,
    .close                = bthid_ll_close,
    .hidinput_input_event = bthid_ll_hidinput_event,
    .parse                = bthid_ll_parse,
};


static int bthid_open(struct inode *inode, struct file *file)
{
    struct bthid_device *p_dev;

    printk("######## bthid_open: ########\n");

    p_dev = kzalloc(sizeof(struct bthid_device), GFP_KERNEL);
    if (!p_dev)
    {
        return -ENOMEM;
    }

    file->private_data = p_dev;
    
    printk("######## bthid_open: done ########\n");
    return 0;
}

static int bthid_release(struct inode *inode, struct file *file)
{
    struct bthid_device *p_dev = file->private_data;

    printk("######## bthid_release: ########\n");
    
    if (p_dev->hid) 
    {
        if (p_dev->hid->status == (HID_STAT_ADDED | HID_STAT_PARSED))
        {
            hidinput_disconnect(p_dev->hid);
        }

        if (p_dev->hid->driver_data != NULL)
        {
            kfree(p_dev->hid->driver_data);
        }

        hid_destroy_device(p_dev->hid);
        p_dev->hid = NULL;
    }

    kfree(p_dev);
    file->private_data = NULL;

    printk("######## bthid_release: done ########\n");
    return 0;
}

static ssize_t bthid_write(struct file *file, const char __user *buffer, size_t count, loff_t *ppos)
{
    unsigned char *buf;
    struct bthid_device *p_dev = file->private_data;

    /*
    printk("######## bthid_write: count = %d ########\n", count);
    */

    if (p_dev->dscp_set == 0)
    {
        printk("bthid_write: Oops, HID report descriptor not configured\n");
        return 0;
    }

    buf = kmalloc(count + 1, GFP_KERNEL);
    if (!buf)
    {
        return -ENOMEM;
    }

    if (copy_from_user(buf, buffer, count))
    {
        kfree(buf);
        return -EFAULT;
    }

    if (p_dev->hid) 
    {
        hid_input_report(p_dev->hid, HID_INPUT_REPORT, buf, count, 1);
    }

    kfree(buf);

    /*
    printk("######## bthid_write: done ########\n");
    */

    return 0;
}

static int bthid_ioctl(struct inode *inode, struct file *file, unsigned int cmd, unsigned long arg)
{
    int ret;
    char *name;
    struct bthid_ctrl *p_ctrl;
    struct bthid_device *p_dev = file->private_data;
    struct bthid_di di;

    printk("######## bthid_ioctl: cmd = %d ########\n", cmd);

    if (p_dev == NULL)
    {
        return -EINVAL;
    }

    if (cmd == BTHID_IOCTL_RPT_NAME) {
        name = kzalloc(BTHID_DEV_NAME_LEN, GFP_KERNEL);

        if (name == NULL)
            return -ENOMEM;

        if (copy_from_user(name, (void __user *)arg, BTHID_DEV_NAME_LEN)) {
            kfree(name);
            return -EFAULT;
        }

        printk("%s: name=%s\n", __func__, name);

        if (!p_dev->hid) {
            p_dev->hid = hid_allocate_device();
            if (p_dev->hid == NULL)
            {
                printk("Oops: Failed to allocation HID device.\n");
                kfree(name);
                return -ENOMEM;
            }
        }

        strncpy(p_dev->hid->name, name, 128);

        kfree(name);
    } else if (cmd == BTHID_IOCTL_RPT_DI) {
        if (copy_from_user(&di, (void __user *) arg, sizeof(struct bthid_di)) != 0)
        {
            return -EFAULT;
        }

        printk("%s: vendor=0x%04x, product=0x%04x, version=0x%04x, country=0x%02x\n", __func__,
                di.vendor_id, di.product_id, di.version, di.ctry_code);
		
        if (!p_dev->hid) {
            p_dev->hid = hid_allocate_device();
            if (p_dev->hid == NULL)
            {
                printk("Oops: Failed to allocation HID device.\n");
                return -ENOMEM;
            }
        }
        p_dev->hid->vendor = di.vendor_id;
        p_dev->hid->product = di.product_id;
        p_dev->hid->version = di.version;
        p_dev->hid->country = di.ctry_code;
    } else if (cmd == BTHID_IOCTL_RPT_DSCP) {
    p_ctrl = kmalloc(sizeof(struct bthid_ctrl), GFP_KERNEL);
    if (p_ctrl == NULL)
    {
        return -ENOMEM;
    }

    if (copy_from_user(p_ctrl, (void __user *) arg, sizeof(struct bthid_ctrl)) != 0)
    {
        kfree(p_ctrl);
        return -EFAULT;
    }

    if (p_ctrl->size <= 0) 
    {
        printk("Oops: Invalid BT HID report descriptor size %d\n", p_ctrl->size); 

        kfree(p_ctrl);
        return -EINVAL;
    }
    
            if (!p_dev->hid) {
    p_dev->hid = hid_allocate_device();
    if (p_dev->hid == NULL)
    {
        printk("Oops: Failed to allocation HID device.\n");

        kfree(p_ctrl);
        return -ENOMEM;
    }
    }
    
    //temperaly use hard code. GB does not support id kl
        if( p_dev->hid->vendor == 0x04E8 && p_dev->hid->product == 0x7021){
            strcpy(p_dev->hid->name, "Vendor_04E8_Product_7021");
            p_dev->hid->name[strlen("Vendor_04E8_Product_7021")] =  '\0';
        } else {
            strcpy(p_dev->hid->name, "Broadcom Bluetooth HID");
            p_dev->hid->name[strlen("Broadcom Bluetooth HID")] =  '\0';	
        }
    
    p_dev->hid->bus         = BUS_BLUETOOTH;
    p_dev->hid->ll_driver   = &bthid_ll_driver;
    p_dev->hid->driver_data = p_ctrl;


    ret = hid_add_device(p_dev->hid);

    printk("hid_add_device: ret = %d, hid->status = %d\n", ret, p_dev->hid->status);

    if (ret != 0)
    {
        printk("Oops: Failed to add HID device");

        kfree(p_ctrl);
        hid_destroy_device(p_dev->hid);
        p_dev->hid = NULL;
        return -EINVAL;
    }
    p_dev->hid->claimed |= HID_CLAIMED_INPUT;

    if (p_dev->hid->status != (HID_STAT_ADDED | HID_STAT_PARSED))
    {
        printk("Oops: Failed to process HID report descriptor");
        return -EINVAL;
    }

    p_dev->dscp_set = 1;
    } else {
        printk("Invlid ioctl value");
        return -EINVAL;
    }

    printk("######## bthid_ioctl: done ########\n");
    return 0;
}


#define BTHID_NAME0 "BtHid"

static const struct hid_device_id bthid_table[] = {
    { HID_BLUETOOTH_DEVICE(HID_ANY_ID, HID_ANY_ID) },
    { }
};

static struct hid_driver bthid_driver = {
    .name     = BTHID_NAME0,
    .id_table = bthid_table,
};


static const struct file_operations bthid_fops = {
    .owner   = THIS_MODULE,
    .open    = bthid_open,
    .release = bthid_release,
    .write   = bthid_write,
    .ioctl   = bthid_ioctl,
};

static struct miscdevice bthid_misc = {
    .name  = BTHID_NAME,
    .minor = BTHID_MINOR,
    .fops  = &bthid_fops,
};


static int __init bthid_init(void)
{
    int ret;

    printk("######## bthid_init: ########\n");

    ret = misc_register(&bthid_misc);
    if (ret != 0)
    {
        printk("Oops, failed to register Misc driver, ret = %d\n", ret);
        return ret;
    }

    ret = hid_register_driver(&bthid_driver);
    if (ret != 0)
    {
        printk("Oops, failed to register HID driver, ret = %d\n", ret);
        return ret;
    }

    printk("######## bthid_init: done ########\n");
    
    return ret;
}

static void __exit bthid_exit(void)
{
    printk("bthid_exit:\n");

    hid_unregister_driver(&bthid_driver);

    misc_deregister(&bthid_misc);
    printk("bthid_exit: done\n");
}

module_init(bthid_init);
module_exit(bthid_exit);
