#include <linux/module.h>
#include <linux/kernel.h>
#include "harddoom2.h"

#define DEBUG(fmt, ...) printk(KERN_INFO "hd2: " fmt "\n", ##__VA_ARGS__)
#define ERROR(fmt, ...) printk(KERN_ERR "hd2: " fmt "\n", ##__VA_ARGS__)
#define DRV_NANE "harddoom2_driver"

static const struct pci_device_id pci_ids[] = {
    { PCI_DEVICE(HARDDOOM2_VENDOR_ID, HARDDOOM2_DEVICE_ID), },
    { /* end: all zeroes */ },
};

struct drvdata {
    void __iomem* bar;
};

int drv_probe(struct pci_dev* dev, const struct pci_device_id* id) {
    int err = 0;
    struct drvdata* data = NULL;

    DEBUG("probe called: %#010x, %#0x10x\n", id->vendor, id->device);

    if (id->vendor != HARDDOOM2_VENDOR_ID || id->device != HARDDOOM2_DEVICE_ID) {
        DEBUG("Wrong device_id!\n");
        return -EINVAL;
    }

    /* TODO: init, multiple devices? */

    if ((err = pci_enable_device(dev))) {
        DEBUG("failed to enable device");
        goto out_enable;
    }

    if ((err = pci_request_regions(dev, DRV_NAME))) {
        DEBUG("failed to request regions");
        goto out_regions;
    }

    data = kmalloc(sizeof(drvdata), GFP_KERNEL);
    if (!data) {
        DEBUG("failed to kmalloc drvdata");
        err = -ENOMEM;
        goto out_drvdata;
    }

    pci_set_drvdata(dev, data);

    data->bar = iomap(dev, 0, 0);
    if (!data->bar) {
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

    /* TODO interrupt handling */

    return 0;

out_dma:
    pci_clear_master(dev);
    pci_iounmap(dev, data->bar);
out_iomap:
    pci_set_drvdata(dev, NULL);
    kfree(data);
out_drvdata:
    pci_release_regions(dev);
out_regions:
    pci_disable_device(dev);
out_enable:
    return err;
}

void drv_remove(struct pci_dev* dev) {
    void __iomem* data;

    /* TODO: cleanup */
    pci_clear_master(dev);
    data = pci_get_drvdata(dev);
    if (!data) {
        ERROR("drv_remove: no drvdata!");
    }
    if (!data->bar) {
        ERROR("drv_remove: no bar!");
    }
    pci_iounmap(dev, data->bar);
    pci_set_drvdata(dev, NULL);
    kfree(data);
    pci_release_regions(dev);
    pci_disable_device(dev);
}

int suspend(struct pci_dev* dev, pm_message_t state) {
    /* TODO cleanup */
    return 0;
}

int drv_resume(struct pci_dev* dev) {
    /* TODO: resume */
}

void drv_shutdown(struct pci_dev* dev) {
    /* TODO cleanup */
}

static const struct pci_driver hd2_drv = {
    .name = DRV_NAME,
    .id_table = pci_ids,
    .probe = drv_probe,
    .remove = drv_remove,
    .suspend = drv_suspend,
    .resume = drv_resume,
    .shutdown = drv_shutdown
};

MODULE_LICENSE("GPL");

int hd2_init(void)
{
    int err;
	DEBUG("Init\n");
    if ((err = pci_register_driver(&hd2_drv))) {
        DEBUG("Failed to register driver\n");
        goto out_reg_drv;
    }

	return 0;

out_reg_drv:
    return err;
}

void hd2_cleanup(void)
{
	DEBUG("Cleanup\n");
    pci_unregister_driver(&hd2_drv);
}

module_init(hd2_init);
module_exit(hd2_cleanup);
