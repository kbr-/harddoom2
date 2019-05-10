#include <linux/module.h>
#include <linux/kernel.h>
#include "harddoom2.h"

MODULE_LICENSE("GPL");

#define DEBUG(fmt, ...) printk(KERN_INFO "hd2: " fmt "\n", ##__VA_ARGS__)
#define ERROR(fmt, ...) printk(KERN_ERR "hd2: " fmt "\n", ##__VA_ARGS__)
#define DRV_NANE "harddoom2_driver"
#define CHRDEV_NAME "doom"
#define DEVICES_LIMIT 256

static const struct pci_device_id pci_ids[] = {
    { PCI_DEVICE(HARDDOOM2_VENDOR_ID, HARDDOOM2_DEVICE_ID), },
    { /* end: all zeroes */ },
};

static struct class doom_class = {
    .name = CHRDEV_NAME,
    .owner = THIS_MODULE,
};

struct device_data {
    int number;
    void __iomem* bar;
    struct cdev cdev;
};

static struct device_data devices[DEVICES_LIMIT];

static struct file_operations doom_ops = {
    .owner = THIS_MODULE,
    .open = doom_open,
    .release = doom_release,
    .unlocked_ioctl = doom_ioctl,
    .compat_ioctl = doom_ioctl,
    .write = doom_write
};

static int dev_counter = 0;
static DEFINE_MUTEX(dev_counter_mut);

int alloc_dev_number() {
    int ret = -ENOSPC;

    /* TODO: interruptible lock? */
    mutex_lock(&dev_counter_mut);
    if (dev_counter < DEVICES_LIMIT) {
        ret = dev_counter++;
    }
    mutex_unlock(&dev_counter_mut);
    return ret;
}

static int pci_probe(struct pci_dev* dev, const struct pci_device_id* id) {
    int err = 0;
    void __iomem bar* = NULL;
    struct device* device = NULL;
    struct device_data* data = NULL;

    DEBUG("probe called: %#010x, %#0x10x", id->vendor, id->device);

    if (id->vendor != HARDDOOM2_VENDOR_ID || id->device != HARDDOOM2_DEVICE_ID) {
        DEBUG("Wrong device_id!");
        return -EINVAL;
    }

    if ((err = pci_enable_device(dev))) {
        DEBUG("failed to enable device");
        goto out_enable;
    }

    if ((err = pci_request_regions(dev, DRV_NAME))) {
        DEBUG("failed to request regions");
        goto out_regions;
    }

    bar = iomap(dev, 0, 0);
    if (!bar) {
        DEBUG("can't map register space");
        err = -ENOMEM;
        goto out_iomap;
    }

    /* DMA support */
    pci_set_master(dev);
    if ((err = pci_set_dma_mask(dev, DMA_BIT_MASK(40)))) {
        DEBUG("WAT: no 40bit DMA, err: %d", err);
        err = -EIO;
        goto out_dma;
    }
    if ((err = pci_set_consistent_dma_mask(dev, DMA_BIT_MASK(40)))) {
        DEBUG("WAT: no 40bit DMA, err: %d (set_consistent_dma_mask)", err);
        err = -EIO;
        goto out_dma;
    }

    /* TODO: free dev numbers, use list? */
    dev_number = alloc_dev_number();
    if (dev_number < 0) {
        DEBUG("failed to allocate device number");
        err = dev_number;
        goto out_dma;
    }
    if (dev_number >= DEVICE_LIMIT) { ERROR("OOPS"); err = -ENOSPC; goto out_dma; }

    data = &devices[dev_number];
    pci_set_drvdata(dev, data);

    data->number = dev_number;
    data->bar = bar;
    cdev_init(&data->cdev, &doom_ops);
    data->cdev.owner = THIS_MODULE;

    /* TODO: setup device before cdev_add */

    if ((err = cdev_add(&data->cdev, doom_major + dev_number, 1))) {
        DEBUG("can't register cdev");
        goto out_cdev_add;
    }

    /* TODO: store device? */
    device = device_create(&doom_class, dev->dev, doom_major + dev_number, 0, CHRDEV_NAME "%d", dev_number);
    if (IS_ERR(device)) {
        DEBUG("can't create device");
        err = PTR_ERR(device);
        goto err_device;
    }

    /* TODO interrupt handling */

    return 0;

err_device:
    cdev_del(&data->cdev);
out_cdev_add:
    pci_set_drvdata(dev, NULL);
    /* TODO */
out_dma:
    pci_clear_master(dev);
    pci_iounmap(dev, data->bar);
out_iomap:
    pci_release_regions(dev);
out_regions:
    pci_disable_device(dev);
out_enable:
    return err;
}

static void pci_remove(struct pci_dev* dev) {
    struct device_data* data = pci_get_drvdata(dev);
    void __iomem* bar = NULL;

    if (!data) {
        ERROR("pci_remove: no pcidata!");
    } else {
        device_destroy(&doom_class, doom_major + data->number);
        cdev_del(&data->cdev);
        bar = data->bar;
        /* TODO: free dev number */
    }

    pci_set_drvdata(dev, NULL);
    pci_clear_master(dev);

    if (!bar) {
        ERROR("pci_remove: no bar!");
    } else {
        pci_iounmap(dev, data->bar);
    }

    pci_release_regions(dev);
    pci_disable_device(dev);
}

static int pci_suspend(struct pci_dev* dev, pm_message_t state) {
    /* TODO cleanup */
    return 0;
}

static int pci_resume(struct pci_dev* dev) {
    /* TODO: resume */
}

static void pci_shutdown(struct pci_dev* dev) {
    /* TODO cleanup */
}

static const struct pci_driver pci_drv = {
    .name = DRV_NAME,
    .id_table = pci_ids,
    .probe = pci_probe,
    .remove = pci_remove,
    .suspend = pci_suspend,
    .resume = pci_resume,
    .shutdown = pci_shutdown
};

static dev_t doom_major;

static int doom_open(struct inode* inode, struct file* file) {
    /* TODO */
}

static int doom_release(struct inode* inode, struct file* file) {
    /* TODO */
}

static int doom_ioctl(struct file* file, unsigned cmd, unsigned long arg) {
    /* TODO */
}

static ssize_t doom_write(struct file* file, const char __user *buf, size_t count, loff_t* off) {
    /* TODO */
}

static int hd2_init(void)
{
    int err = 0;

	DEBUG("Init");
    if ((err = alloc_chrdev_region(&doom_major, DEVICES_LIMIT, CHRDEV_NAME))) {
        DEBUG("failed to alloc chrdev region");
        goto out_alloc_reg;
    }

    if ((err = class_register(&doom_class))) {
        DEBUG("failed to register class");
        goto out_class;
    }

    if ((err = pci_register_driver(&pci_drv))) {
        DEBUG("Failed to register pci driver");
        goto out_reg_drv;
    }

	return 0;

out_reg_drv:
    class_unregister(&doom_class);
out_class:
    unregister_chrdev_region(&doom_major, DEVICES_LIMIT);
out_alloc_reg:
    return err;
}

static void hd2_cleanup(void)
{
	DEBUG("Cleanup");
    pci_unregister_driver(&pci_drv);
    class_unregister(&doom_class);
    unregister_chrdev_region(doom_major, DEVICES_LIMIT);
}

module_init(hd2_init);
module_exit(hd2_cleanup);
