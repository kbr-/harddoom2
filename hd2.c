#include <linux/module.h>
#include <linux/kernel.h>

#include "harddoom2.h"

#include "counter.h"
#include "buffer.h"
#include "hd2.h"

MODULE_LICENSE("GPL");

#define DEBUG(fmt, ...) printk(KERN_INFO "hd2: " fmt "\n", ##__VA_ARGS__)
#define ERROR(fmt, ...) printk(KERN_ERR "hd2: " fmt "\n", ##__VA_ARGS__)
#define DRV_NANE "harddoom2_driver"
#define CHRDEV_NAME "HardDoom_][™"
#define DEVICES_LIMIT 256
#define MAX_KMALLOC (128 * 1024)
#define PAGE_SIZE (4 * 1024)
#define MAX_BUFFER_SIZE (4 * 1024 * 1024)
#define NUM_USER_BUFS 7
#define PING_PERIOD (2048 + 32)
#define NUM_INTR_BITS 15

/* 128K */
#define CMD_BUF_SIZE (MAX_BUFFER_SIZE / (8 * 4))

static const struct pci_device_id pci_ids[] = {
    { PCI_DEVICE(HARDDOOM2_VENDOR_ID, HARDDOOM2_DEVICE_ID), },
    { /* end: all zeroes */ },
};

static struct class doom_class = {
    .name = CHRDEV_NAME,
    .owner = THIS_MODULE,
};

struct harddoom2 {
    int number;
    void __iomem* bar;
    struct cdev cdev;
    struct pci_dev* pdev;

    struct buffer cmd_buff;

    uint32_t write_idx;

    struct counter batch_cnt;

    struct counter last_fence_cnt;
    struct counter last_fence_wait;

    /* Used to wait for free space in the command buffer. */
    wait_queue_head_t write_wq;

    /* Used to wait for commands using a particular buffer to finish. */
    wait_queue_head_t fence_wq;

    /* Manages the lifetime of buffers used by the device.
       When a SETUP command is sent to the command queue, we remember the set of changed buffers
       in the queue, increasing their reference counts. We periodically clear the queue,
       removing buffers which have been replaced and decreasing their reference counts. */
    struct list_head changes_queue;

    /* Buffers used in the last SETUP command sent to the command buffer. */
    struct file* curr_bufs[NUM_USER_BUFS];
};

struct buffer_change {
    /* NULL pointer represents no change. */
    /* Non-NULL pointer indicates what buffer was used BEFORE the change. */
    struct file* bufs[NUM_USER_BUFS];

    /* Number of the batch in which this set of buffers was removed */
    struct counter cnt;

    struct list_head list;
}

static struct harddoom2 devices[DEVICES_LIMIT];

// static int dev_counter = 0;
// static DEFINE_MUTEX(dev_counter_mut);

int alloc_dev_number() {
    int ret = -ENOSPC;

    /* TODO lock */
    if (dev_counter < DEVICES_LIMIT) {
        ret = dev_counter++;
    }
    return ret;
}

typedef void (*doom_irq_handler_t)(struct harddoom2*, uint32_t);

void handle_pong_async(struct harddoom2* data, uint32_t bit) {
    DEBUG("pong_async");

    wake_up(&data->write_wq);
}

void handle_impossible(struct harddoom2* data, uint32_t bit) {
    ERROR("Impossible interrupt: %u", bit);
}

irqreturn_t doom_irq_handler(int irq, void* _hd2) {
    static const uint32_t intr_bits[NUM_INTR_BITS] = {
        HARDDOOM2_INTR_FENCE, HARDDOOM2_INTR_PONG_SYNC, HARDDOOM2_INTR_PONG_ASYNC,
        HARDDOOM2_INTR_FE_ERROR, HARDDOOM2_INTR_CMD_OVERFLOW, HARDDOOM2_INTR_SURF_DST_OVERFLOW,
        HARDDOOM2_INTR_SURF_SRC_OVERFLOW, HARDDOOM2_INTR_PAGE_FAULT_CMD, HARDDOOM2_INTR_PAGE_FAULT_SURF_DST,
        HARDDOOM2_PAGE_FAULT_SURF_SRC, HARDDOOM2_PAGE_FAULT_TEXTURE, HARDDOOM2_PAGE_FAULT_FLAT,
        HARDDOOM2_PAGE_FAULT_TRANSLATION, HARDDOOM2_PAGE_FAULT_COLORMAP, HARDDOOM2_PAGE_FAULT_TRANMAP };

    static const doom_irq_handler_t[NUM_INTR_BITS] = {
        handle_fence, handle_impossible, handle_pong_async,
        [3 ... (NUM_INTR_BITS - 1)] = handle_impossible };

    struct harddoom2* hd2 = _hd2;
    BUG_ON(!hd2);

    /* TODO synchro */
    uint32_t active = ioread32(hd2->bar + HARDDOOM2_INTR);
    iowrite32(active, hd2->bar + HARDDOOM2_INTR);

    int served = 0;
    for (int i = 0; i < NUM_INTR_BITS; ++i) {
        uint32_t bit = intr_bits[i];
        if (bit & active) {
            intr_handlers[i](hd2, bit);
            served = 1;
        }
    }

    if (served) {
        return IRQ_HANDLED;
    }

    return IRQ_NONE;
}

void reset_device(void __iomem* bar, dma_addr_t cmds_page_table) {
    iowrite32(0, bar + HARDDOOM2_FE_CODE_ADDR);
    for (int i = 0; i < sizeof(doomcode2) / sizeof(uint32_t); ++i) {
        iowrite32(doomcode2[i], bar + HARDDOOM2_FE_CODE_WINDOW);
    }
    iowrite32(HARDDOOM2_RESET_ALL, bar + HARDDOOM2_RESET);

    iowrite(cmds_page_table >> 8, bar + HARDDOOM2_CMD_PT);
    iowrite(0, bar + HARDDOOM2_CMD_READ_IDX);
    iowrite(0, bar + HARDDOOM2_CMD_WRITE_IDX);
    iowrite(HARDDOOM2_INTR_MASK, bar + HARDDOOM2_INTR);
    iowrite(0, bar + HARDDOOM2_FENCE_COUNTER);
    iowrite(HARDDOOM2_ENABLE_ALL, bar + HARDDOOM2_ENABLE);
}

void device_off(void __iomem* bar) {
    iowrite32(0, bar + HARDDOOM2_ENABLE);
    iowrite32(0, bar + HARDDOOM2_INTR_ENABLE);
    ioread32(bar + HARDDOOM2_ENABLE);
}

void free_all_buffers(struct harddoom2* hd2) {
    free_buffer(&hd2->cmd_buff);

    for (int i = 0; i < NUM_USER_BUFS; ++i) {
        fput(hd2->last_bufs[i]);
    }

    while (!list_empty(&hd2->changes_queue)) {
        struct buffer_change* change = list_first_entry(&hd2->changes_queue, struct buffer_change, list);
        for (int i = 0; i < NUM_USER_BUFS; ++i) {
            fput(change->bufs[i]);
        }

        list_del(&change->list);
        kfree(change);
    }
}

static int pci_probe(struct pci_dev* pdev, const struct pci_device_id* id) {
    int err = 0;
    void __iomem bar* = NULL;
    struct device* device = NULL;
    struct harddoom2* hd2 = NULL;
    int dev_number = DEVICE_LIMIT;

    DEBUG("probe called: %#010x, %#0x10x", id->vendor, id->device);

    if (id->vendor != HARDDOOM2_VENDOR_ID || id->device != HARDDOOM2_DEVICE_ID) {
        DEBUG("Wrong device_id!");
        return -EINVAL;
    }

    if ((err = pci_enable_device(pdev))) {
        DEBUG("failed to enable device");
        goto out_enable;
    }

    if ((err = pci_request_regions(pdev, DRV_NAME))) {
        DEBUG("failed to request regions");
        goto out_regions;
    }

    bar = iomap(pdev, 0, 0);
    if (!bar) {
        DEBUG("can't map register space");
        err = -ENOMEM;
        goto out_iomap;
    }

    /* DMA support */
    pci_set_master(pdev);
    if ((err = pci_set_dma_mask(pdev, DMA_BIT_MASK(40)))) {
        DEBUG("WAT: no 40bit DMA, err: %d", err);
        err = -EIO;
        goto out_dma;
    }
    if ((err = pci_set_consistent_dma_mask(pdev, DMA_BIT_MASK(40)))) {
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

    hd2 = &devices[dev_number];
    pci_set_drvdata(pdev, hd2);

    memset(hd2, 0, sizeof(harddoom2));
    hd2->number = dev_number;
    hd2->bar = bar;
    cdev_init(&hd2->cdev, &context_ops);
    hd2->cdev.owner = THIS_MODULE;
    hd2->pdev = pdev;

    if ((err = init_buffer(&hd2->cmd_buff, &pdev->dev, MAX_BUFFER_SIZE))) {
        DEBUG("can't init cmd_buff");
        goto out_cmd_buff;
    }

    init_waitqueue_head(&hd2->write_wq);
    init_waitqueue_head(&hd2->fence_wq);
    INIT_LIST_HEAD(&hd2->changes_queue);

    reset_device(bar, hd2->cmd_buff->page_table_dev);

    if ((err = cdev_add(&hd2->cdev, doom_major + dev_number, 1))) {
        DEBUG("can't register cdev");
        goto out_cdev_add;
    }

    device = device_create(&doom_class, &pdev->dev, doom_major + dev_number, 0, CHRDEV_NAME "%d", dev_number);
    if (IS_ERR(device)) {
        DEBUG("can't create device");
        err = PTR_ERR(device);
        goto err_device;
    }

    if ((err = request_irq(pdev->irq, doom_irq_handler, IRQF_SHARED, CHRDEV_NAME, hd2))) {
        DEBUG("can't setup interrupt handler");
        goto err_irq;
    }

    return 0;

err_irq:
    device_destroy(&doom_class, doom_major + dev_number);
err_device:
    cdev_del(&hd2->cdev);
out_cdev_add:
    device_off(bar);
    free_all_buffers(hd2);
out_cmd_buff:
    pci_set_drvdata(pdev, NULL);
    /* TODO */
out_dma:
    pci_clear_master(pdev);
    pci_iounmap(pdev, hd2->bar);
out_iomap:
    pci_release_regions(pdev);
out_regions:
    pci_disable_device(pdev);
out_enable:
    return err;
}

static void pci_remove(struct pci_dev* dev) {
    struct harddoom2* hd2 = pci_get_drvdata(dev);
    void __iomem* bar = NULL;

    if (!hd2) {
        ERROR("pci_remove: no pcidata!");
    } else {
        free_irq(dev->irq, hd2);
        if (hd2->number >= 0 && hd2->number < DEVICES_LIMIT) {
            device_destroy(&doom_class, doom_major + hd2->number);
        } else {
            ERROR("pci_remove: OOPS");
            if (&devices[hd2->number] != hd2) {
                ERROR("pci_remove: double OOPS");
            }
        }
        hd2->number = -1;
        cdev_del(&hd2->cdev);
        bar = hd2->bar;

        /* TODO: what if device gone */
        device_off(bar);
        free_all_buffers(hd2);

        /* TODO: free dev number */
    }

    pci_set_drvdata(dev, NULL);
    pci_clear_master(dev);

    if (!bar) {
        ERROR("pci_remove: no bar!");
    } else {
        pci_iounmap(dev, hd2->bar);
    }

    pci_release_regions(dev);
    pci_disable_device(dev);
}

static int pci_suspend(struct pci_dev* dev, pm_message_t state) {
    DEBUG("suspend");
    /* TODO cleanup */
    return 0;
}

static int pci_resume(struct pci_dev* dev) {
    DEBUG("resume");
    /* TODO: resume */
}

static void pci_shutdown(struct pci_dev* dev) {
    DEBUG("shutdown");
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

static int setup_buffers(struct harddoom2* hd2, struct fd* bufs, size_t write_idx, uint32_t extra_flags) {
    int has_change = 0;
    int is_diff = 0;

    int i;
    for (i = 0; i < NUM_USER_BUFS; ++i) {
        if (bufs[i].file != hd2->curr_bufs[i]) {
            is_diff = 1;
            if (hd2->curr_bufs[i]) {
                has_change = 1;
                break;
            }
        }
    }

    if (!is_diff) {
        return 0;
    }

    struct buffer_change* change = NULL;
    if (has_change) {
        change = kzalloc(sizeof(struct buffer_change), GFP_KERNEL);
        if (!change) {
            return -ENOMEM;
        }
        change->cnt = hd2->batch_cnt;

        list_add_tail(&change->list, &hd2->changes_queue);
    }

    struct cmd dev_cmd = make_setup(bufs, extra_flags);
    write_cmd(hd2, &dev_cmd, write_idx);

    for (i = 0; i < NUM_USER_BUFS; ++i) {
        if (bufs[i].file == hd2->curr_bufs[i]) continue;

        if (bufs[i].file) {
            fget(bufs[i].file);
        }
        if (hd2->curr_bufs[i]) {
            change->bufs[i] = hd2->curr_bufs[i];
        }
        hd2->curr_bufs[i] = bufs[i].file;
    }

    return 1;
}

static void collect_buffers(struct harddoom2* hd2) {
    struct counter cnt = get_curr_fence_cnt(hd2);
    while (!list_empty(&hd2->changes_queue)) {
        struct buffer_change* change = list_first_entry(&hd2->changes_queue, struct buffer_change, list);

        if (!cnt_ge(counter, change->cnt)) {
            return;
        }

        int i;
        for (i = 0; i < NUM_USER_BUFS; ++i) {
            if (change->bufs[i]) {
                fput(change->bufs[i]);
            }
        }

        list_del(&change->list);
        kfree(change);
    }
}

struct cmd {
    uint32_t data[8];
};

static void make_cmd(struct cmd* dev_cmd, const struct doomdev2_cmd* user_cmd, uint32_t extra_flags) {
    switch (user_cmd->type) {
    case DOOMDEV2_CMD_TYPE_FILL_RECT:
        dev_cmd->data = {
            HARDDOOM2_CMD_W0(HARDDOOM2_CMD_TYPE_FILL_RECT, extra_flags),
            0,
            HARDDOOM2_CMD_W3(cmd.pos_x, cmd.pos_y),
            0,
            0,
            0,
            HARDDOOM2_CMD_W6_A(cmd.width, cmd.height, cmd.fill_color),
            0
        };
        return;
    case DOOMDEV2_CMD_TYPE_DRAW_LINE:
        dev_cmd->data = {
            HARDDOOM2_CMD_W0(HARDDOOM2_CMD_TYPE_DRAW_LINE, extra_flags),
            0,
            HARDDOOM2_CMD_W3(cmd.pos_a_x, cmd.pos_a_y),
            HARDDOOM2_CMD_W3(cmd.pos_b_x, cmd.pos_b_y),
            0,
            0,
            HARDDOOM2_CMD_W6_A(0, 0, cmd.fill_color),
            0
        };
        return;
    default:
        /* TODO other cmds */
        dev_cmd->data = {
            HARDDOOM2_CMD_W0(HARDDOOM2_CMD_TYPE_FILL_RECT, extra_flags),
            0,
            HARDDOOM_CMD_W3(0, 0),
            0,
            0,
            0,
            HARDDOOM2_CMD_W6_A(1, 1, 0),
            0
        };
    }
}

static void write_cmd(struct harddoom2* hd2, struct cmd* cmd, size_t write_idx) {
    struct buffer* buff = &hd2->cmd_buff;
    BUG_ON(write_idx >= CMD_BUF_SIZE);
    BUG_ON(buff->size != CMD_BUF_SIZE);

    size_t dst_pos = write_idx * 8;
    return write_buffer(buff, cmd->data, dst_pos, sizeof(cmd->data));
}

static struct cmd make_setup(struct fd* bufs, uint32_t extra_flags) {
    static const uint32_t bufs_flags[NUM_USER_BUFS] = {
        HARDDOOM2_CMD_FLAG_SETUP_SURF_DST, HARDDOOM2_CMD_FLAG_SETUP_SURF_SRC,
        HARDDOOM2_CMD_FLAG_SETUP_TEXTURE, HARDDOOM2_CMD_FLAG_SETUP_FLAT, HARDDOOM2_CMD_FLAG_SETUP_COLORMAP,
        HARDDOOM2_CMD_FLAG_SETUP_TRANSLATION, HARDDOOM2_CMD_FLAG_SETUP_TRANMAP };

    int i;
    for (i = 0; i < NUM_USER_BUFS; ++i) {
        if (bufs[i].file) {
            extra_flags |= bufs_flags[i];
        }
    }

    uint16_t dst_width = 0;
    uint16_t src_width = 0;
    {
        struct file* f;
        if ((f = bufs[0].file)) {
            struct buffer* buff = (struct buffer*)f->private_data;
            dst_width = buff->width;
        }
        if ((f = bufs[1].file)) {
            struct buffer* buff = (struct buffer*)f->private_data;
            src_width = buff->width;
        }
    }

    struct cmd hd_cmd;
    hd_cmd.data[0] = HARDDOOM2_CMD_W0_SETUP(HARDDOOM2_CMD_TYPE_SETUP, extra_flags, dst_width, src_width);
    for (int i = 0; i < NUM_USER_BUFFS; ++i) {
        struct file* f = bufs[i].file;
        hd_cmd.data[i+1] = f ? (((struct buffer*)f->private_data)->page_table_dev >> 8) : 0;
    }

    return hd_cmd;
}

static uint32_t get_cmd_buf_space(struct harddoom2* hd2) {
    return ioread32(hd2->bar + HARDDOOM2_CMD_READ_IDX) - hd2->write_idx - 1;
}

void enable_intr(struct harddoom2* hd2, uint32_t intr) {
    iowrite32(ioread32(hd2->bar + HARDDOOM2_INTR_ENABLE)  | intr, hd2->bar + HARDDOOM2_INTR_ENABLE);
}

void disable_intr(struct harddoom2* hd2, uint32_t intr) {
    iowrite32(ioread32(hd2->bar + HARDDOOM2_INTR_ENABLE)  & ~intr, hd2->bar + HARDDOOM2_INTR_ENABLE);
}

void deactivate_intr(struct harddoom2* hd2, uint32_t intr) {
    iowrite(intr, hd2->bar + HARDDOOM2_INTR);
}

ssize_t harddoom2_write(struct harddoom2* hd2, struct fd* bufs, const struct doomdev2_cmd* cmds, size_t num_cmds) {
    update_last_fence_cnt(hd2);

    /* TODO mutex lock */
    while (get_cmd_buf_space(hd2) < 2) {
        deactivate_intr(HARDDOOM2_INTR_PONG_ASYNC);
        if (get_cmd_buf_space(hd2) >= 2) {
            break;
        }
        enable_intr(hd2, HARDDOOM2_INTR_PONG_ASYNC);
        /* TODO unlock */

        wait_event(&hd2->write_wq, get_cmd_buf_space(hd2) >= 2);

        /* TODO mutex lock */
    }

    disable_intr(hd2, HARDDOOM2_INTR_PONG_ASYNC);
    /* If anyone was waiting on the event queue, let them enable the interrupt again. */
    wake_all(&hd2->write_wq);

    uint32_t space = get_cmd_buf_space(hd2);
    uint32_t write_idx = hd2->write_idx;
    uint32_t extra_flags = (write_idx % PING_PERIOD) ? 0 : HARDDOOM2_CMD_FLAG_PING_ASYNC;

    int set = setup_buffers(hd2, bufs, write_idx, extra_flags);
    if (set < 0) {
        DEBUG("write: could not setup");
        goto out_setup;
    } else if (set) {
        --space;
        write_idx = (write_idx + 1) % CMD_BUF_SIZE;
        extra_flags = (write_idx % PING_PERIOD) ? 0 : HARDDOOM2_CMD_FLAG_PING_ASYNC;
    }

    if (num_cmds > space) {
        num_cmds = space;
    }

    struct cmd dev_cmd;
    size_t it;
    for (it = 0; it < num_cmds - 1; ++it) {
        make_cmd(&dev_cmd, &cmds[it], extra_flags);
        write_cmd(hd2, &dev_cmd, write_idx);

        write_idx = (write_idx + 1) % CMD_BUF_SIZE;
        extra_flags = (write_idx % PING_PERIOD) ? 0 : HARDDOOM2_CMD_FLAG_PING_ASYNC;
    }

    extra_flags |= HARDDOOM2_CMD_FLAG_FENCE;
    make_cmd(&dev_cmd, &cmds[it], extra_flags);
    write_cmd(hd2, &dev_cmd, write_idx);

    hd2->write_idx = write_idx;
    iowrite32(write_idx, hd2->bar + HARDDOOM2_CMD_WRITE_IDX);
    cnt_incr(&hd2->batch_cnt);

    ((struct buffer*)hd2->curr_bufs[0]->private_data)->last_write = hd2->batch_cnt;

    /* TODO: update buffers that were actually used */
    int i;
    for (i = 0; i < NUM_USER_BUFS; ++i) {
        if(hd2->curr_bufs[i]) {
            ((struct buffer*)hd2->curr_bufs[i]->private_data)->last_use = hd2->batch_cnt;
        }
    }

    /* TODO do this outside lock? */
    collect_buffers(ctx->hd2);

    /* TODO unlock */

    return num_cmds;

out_setup:
    /* TODO unlock */
    return set;
}

int harddoom2_create_surface(struct harddoom2* hd2, struct doomdev2_ioctl_create_surface __user* _params) {
    int err;

    struct doomdev2_ioctl_create_surface params;
    if (copy_from_user(&params, _params, sizeof(struct doomdev2_ioctl_create_surface))) {
        DEBUG("create surface copy_from_user fail");
        return -EFAULT;
    }

    DEBUG("create_surface: %u, %u", (unsigned)params.width, (unsigned)params.height);
    if (params.width < 64 || params.width > 2048 || params.width % 64 != 0
            || params.height < 1 || params.height > 2048) {
        return -EINVAL;
    }

    return make_buffer(ctx, params,width * params.height, params.width, params.height);
}

int harddoom2_create_buffer(struct context* ctx, struct doomdev2_ioctl_create_buffer __user* _params) {
    int err;

    struct doomdev2_ioctl_create_buffer params;
    if (copy_from_user(&params, _params, sizeof(struct doomdev2_ioctl_create_buffer))) {
        DEBUG("create_buffer copy_from_user fail");
        return -EFAULT;
    }

    DEBUG("create_buffer: %u", params.size);
    if (params.size > MAX_BUFFER_SIZE) {
        return -EINVAL;
    }

    return make_buffer(ctx, params.size, 0, 0);
}
