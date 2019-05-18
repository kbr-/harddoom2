#include <linux/module.h>
#include <linux/kernel.h>

#include "doomcode2.h"
#include "harddoom2.h"

#include "context.h"
#include "counter.h"
#include "buffer.h"
#include "hd2.h"

MODULE_LICENSE("GPL");

#define CHRDEV_PREFIX "doom"
#define DEVICES_LIMIT 256
#define NUM_USER_BUFS 7
#define PING_PERIOD 2048
#define HARDDOOM2_ADDR_SIZE 40

/* 128K */
#define CMD_BUF_SIZE (MAX_BUFFER_SIZE / HARDDOOM2_CMD_SEND_SIZE)

static const struct pci_device_id pci_ids[] = {
    { PCI_DEVICE(HARDDOOM2_VENDOR_ID, HARDDOOM2_DEVICE_ID), },
    { /* end: all zeroes */ },
};

static struct class doom_class = {
    .name = DRV_NAME,
    .owner = THIS_MODULE,
};

/* Represents a single device. */
struct harddoom2 {
    int number;
    void __iomem* bar;
    struct pci_dev* pdev;
    struct cdev cdev;

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

static int dev_counter = 0;
// static DEFINE_MUTEX(dev_counter_mut);

static int alloc_dev_number() {
    int ret = -ENOSPC;

    /* TODO lock */
    if (dev_counter < DEVICES_LIMIT) {
        ret = dev_counter++;
    }
    return ret;
}

static typedef void (*doom_irq_handler_t)(struct harddoom2*, uint32_t);

static void handle_fence(struct harddoom2* hd2, uint32_t bit) {
    DEBUG("handle fence");

    wake_up(&hd2->fence_wq);
}

static void handle_pong_async(struct harddoom2* hd2, uint32_t bit) {
    DEBUG("pong_async");

    wake_up(&hd2->write_wq);
}

static void handle_impossible(struct harddoom2* hd2, uint32_t bit) {
    ERROR("Impossible interrupt: %u", bit);
    BUG();
}

static irqreturn_t doom_irq_handler(int irq, void* _hd2) {
    static const uint32_t intr_bits[] = {
        HARDDOOM2_INTR_FENCE, HARDDOOM2_INTR_PONG_SYNC, HARDDOOM2_INTR_PONG_ASYNC,
        HARDDOOM2_INTR_FE_ERROR, HARDDOOM2_INTR_CMD_OVERFLOW, HARDDOOM2_INTR_SURF_DST_OVERFLOW,
        HARDDOOM2_INTR_SURF_SRC_OVERFLOW, HARDDOOM2_INTR_PAGE_FAULT_CMD, HARDDOOM2_INTR_PAGE_FAULT_SURF_DST,
        HARDDOOM2_PAGE_FAULT_SURF_SRC, HARDDOOM2_PAGE_FAULT_TEXTURE, HARDDOOM2_PAGE_FAULT_FLAT,
        HARDDOOM2_PAGE_FAULT_TRANSLATION, HARDDOOM2_PAGE_FAULT_COLORMAP, HARDDOOM2_PAGE_FAULT_TRANMAP };

    static const uint32_t NUM_INTR_BITS = sizeof(intr_bits) / size_of(uint32_t);
    _Static_assert(NUM_INTR_BITS == 15, "num intr bits");

    static const doom_irq_handler_t intr_handlers[NUM_INTR_BITS] = {
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

static void reset_device(void __iomem* bar, dma_addr_t cmds_page_table) {
    static const int DOOMCODE2_LEN = sizeof(doomcode2) / sizeof(uint32_t);

    iowrite32(0, bar + HARDDOOM2_FE_CODE_ADDR);
    for (int i = 0; i < DOOMCODE2_LEN; ++i) {
        iowrite32(doomcode2[i], bar + HARDDOOM2_FE_CODE_WINDOW);
    }
    iowrite32(HARDDOOM2_RESET_ALL, bar + HARDDOOM2_RESET);

    iowrite(cmds_page_table >> 8, bar + HARDDOOM2_CMD_PT);
    iowrite(CMD_BUF_SIZE, bar + HARDDOOM2_CMD_SIZE);
    iowrite(0, bar + HARDDOOM2_CMD_READ_IDX);
    iowrite(0, bar + HARDDOOM2_CMD_WRITE_IDX);

    iowrite(HARDDOOM2_INTR_MASK, bar + HARDDOOM2_INTR);
    iowrite(HARDDOOM2_INTR_MASK & ~HARDDOOM2_INTR_PONG_ASYNC, bar + HARDDOOM2_INTR_ENABLE);

    iowrite(0, bar + HARDDOOM2_FENCE_COUNTER);

    iowrite(HARDDOOM2_ENABLE_ALL, bar + HARDDOOM2_ENABLE);
}

static void device_off(void __iomem* bar) {
    iowrite32(0, bar + HARDDOOM2_ENABLE);
    iowrite32(0, bar + HARDDOOM2_INTR_ENABLE);
    ioread32(bar + HARDDOOM2_ENABLE);
}

/* Make sure to call device_off before freeing the buffers. */
static void free_buffers(struct harddoom2* hd2) {
    free_buffer(&hd2->cmd_buff);

    release_user_bufs(hd2->curr_bufs);

    while (!list_empty(&hd2->changes_queue)) {
        struct buffer_change* change = list_first_entry(&hd2->changes_queue, struct buffer_change, list);

        release_user_bufs(change->bufs);

        list_del(&change->list);
        kfree(change);
    }
}

static int pci_probe(struct pci_dev* pdev, const struct pci_device_id* id) {
    int err;

    DEBUG("probe called: %#010x, %#0x10x", id->vendor, id->device);

    if (id->vendor != HARDDOOM2_VENDOR_ID || id->device != HARDDOOM2_DEVICE_ID) {
        DEBUG("Wrong device_id!");
        return -EINVAL;
    }

    if ((err = pci_enable_device(pdev))) {
        DEBUG("can't enable device");
        return err;
    }

    if ((err = pci_request_regions(pdev, DRV_NAME))) {
        DEBUG("can't request regions");
        goto out_regions;
    }

    void __iomem* bar = pci_iomap(pdev, 0, 0);
    if (!bar) {
        DEBUG("can't map register space");
        err = -ENOMEM;
        goto out_iomap;
    }

    /* DMA support */
    pci_set_master(pdev);

    if ((err = pci_set_dma_mask(pdev, DMA_BIT_MASK(HARDDOOM2_ADDR_SIZE)))) {
        DEBUG("can't set dma mask, err: ", err);
        err = -EIO;
        goto out_dma;
    }
    if ((err = pci_set_consistent_dma_mask(pdev, DMA_BIT_MASK(HARDDOOM2_ADDR_SIZE)))) {
        DEBUG("can't set consistent dma mask, err: ", err);
        err = -EIO;
        goto out_dma;
    }

    /* TODO: free dev numbers, use list? */
    if ((err = alloc_dev_number()) < 0) {
        DEBUG("can't allocate device number");
        goto out_dma;
    }

    int dev_number = err;
    BUG_ON(dev_number >= DEVICE_LIMIT);

    struct harddoom2* hd2 = &devices[dev_number];

    memset(hd2, 0, sizeof(struct harddoom2));
    hd2->number = dev_number;
    hd2->bar = bar;
    hd2->pdev = pdev;

    if ((err = init_buffer(&hd2->cmd_buff, &pdev->dev, MAX_BUFFER_SIZE))) {
        DEBUG("can't init cmd_buff");
        goto out_cmd_buff;
    }

    init_waitqueue_head(&hd2->write_wq);
    init_waitqueue_head(&hd2->fence_wq);
    INIT_LIST_HEAD(&hd2->changes_queue);

    pci_set_drvdata(pdev, hd2);

    if ((err = request_irq(pdev->irq, doom_irq_handler, IRQF_SHARED, DRV_NAME, hd2))) {
        DEBUG("can't setup interrupt handler");
        goto err_irq;
    }

    reset_device(bar, hd2->cmd_buff->page_table_dev);

    cdev_init(&hd2->cdev, &context_ops);
    hd2->cdev.owner = THIS_MODULE;

    if ((err = cdev_add(&hd2->cdev, doom_major + dev_number, 1))) {
        DEBUG("can't register cdev");
        goto out_cdev_add;
    }

    struct device* dev = device_create(&doom_class, &pdev->dev,
            doom_major + dev_number, 0, CHRDEV_PREFIX "%d", dev_number);
    if (IS_ERR(dev)) {
        DEBUG("can't create device");
        err = PTR_ERR(dev);
        goto err_device;
    }

    return 0;

err_device:
    cdev_del(&hd2->cdev);
out_cdev_add:
    device_off(bar);
    free_irq(pdev->irq, hd2);
err_irq:
    pci_set_drvdata(pdev, NULL);
    free_buffers(hd2);
out_cmd_buff:
    /* TODO free dev numbers */
out_dma:
    pci_clear_master(pdev);

    pci_iounmap(pdev, bar);
out_iomap:
    pci_release_regions(pdev);
out_regions:
    pci_disable_device(pdev);
    return err;
}

static void pci_remove(struct pci_dev* pdev) {
    struct harddoom2* hd2 = pci_get_drvdata(pdev);
    void __iomem* bar = NULL;

    if (!hd2) {
        ERROR("pci_remove: no pcidata!");
    } else {
        BUG_ON(hd2->number < 0 || hd2->number >= DEVICES_LIMIT);
        BUG_ON(&devices[hd2->number] != hd2);

        bar = hd2->bar;

        device_destroy(&doom_class, doom_major + hd2->number);
        cdev_del(&hd2->cdev);
        device_off(bar);
        free_irq(pdev->irq, hd2);
        pci_set_drvdata(pdev, NULL);
        free_buffers(hd2);
    }

    /* TODO: free dev number */
    pci_clear_master(pdev);

    if (!bar) {
        ERROR("pci_remove: no bar!");
    } else {
        pci_iounmap(pdev, bar);
    }

    pci_release_regions(pdev);
    pci_disable_device(pdev);
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
    int err;

	DEBUG("Init");
    if ((err = alloc_chrdev_region(&doom_major, DEVICES_LIMIT, DRV_NAME))) {
        DEBUG("failed to alloc chrdev region");
        return err;
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
    unregister_chrdev_region(doom_major, DEVICES_LIMIT);
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

struct cmd {
    uint32_t data[HARDDOOM2_CMD_SEND_SIZE];
};

static void write_cmd(struct harddoom2* hd2, struct cmd* cmd, size_t write_idx) {
    struct buffer* buff = &hd2->cmd_buff;
    BUG_ON(write_idx >= CMD_BUF_SIZE);
    BUG_ON(buff->size != CMD_BUF_SIZE * HARDDOOM2_CMD_SEND_SIZE);

    size_t dst_pos = write_idx * HARDDOOM2_CMD_SEND_SIZE;
    return write_buffer(buff, cmd->data, dst_pos, HARDDOOM2_CMD_SEND_SIZE);
}

static int setup_buffers(struct harddoom2* hd2, struct fd* bufs) {
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

    for (i = 0; i < NUM_USER_BUFS; ++i) {
        if (bufs[i].file == hd2->curr_bufs[i]) continue;

        if (bufs[i].file) {
            fget(bufs[i].file);
        }
        if (hd2->curr_bufs[i]) {
            BUG_ON(!change);
            change->bufs[i] = hd2->curr_bufs[i];
        }
        hd2->curr_bufs[i] = bufs[i].file;
    }

    struct cmd dev_cmd = make_setup(hd2->curr_bufs, (hd2->write_idx % PING_PERIOD) ? 0 : HARDDOOM2_CMD_FLAG_PING_ASYNC);
    write_cmd(hd2, &dev_cmd, hd2->write_idx);
    hd->write_idx = (hd->write_idx + 1) % CMD_BUF_SIZE;

    return 1;
}

static void release_user_bufs(struct file** bufs) {
    for (int i = 0; i < NUM_USER_BUFS; ++i) {
        if (bufs[i]) {
            fput(bufs[i]);
        }
    }
}

static void collect_buffers(struct harddoom2* hd2) {
    struct counter cnt = get_curr_fence_cnt(hd2);
    while (!list_empty(&hd2->changes_queue)) {
        struct buffer_change* change = list_first_entry(&hd2->changes_queue, struct buffer_change, list);

        if (!cnt_ge(counter, change->cnt)) {
            return;
        }

        release_user_bufs(change->bufs);

        list_del(&change->list);
        kfree(change);
    }
}

static void make_cmd(struct cmd* dev_cmd, const struct doomdev2_cmd* user_cmd, uint32_t extra_flags) {
    switch (user_cmd->type) {
    case DOOMDEV2_CMD_TYPE_FILL_RECT: {
        struct doomdev2_cmd_fill_rect* cmd = &user_cmd->fill_rect;
        dev_cmd->data = {
            HARDDOOM2_CMD_W0(HARDDOOM2_CMD_TYPE_FILL_RECT, extra_flags),
            0,
            HARDDOOM2_CMD_W3(cmd->pos_x, cmd->pos_y),
            0,
            0,
            0,
            HARDDOOM2_CMD_W6_A(cmd->width, cmd->height, cmd->fill_color),
            0
        };
        return;
    }
    case DOOMDEV2_CMD_TYPE_DRAW_LINE: {
        struct doomdev2_cmd_draw_line* cmd = &user_cmd->draw_line;
        dev_cmd->data = {
            HARDDOOM2_CMD_W0(HARDDOOM2_CMD_TYPE_DRAW_LINE, extra_flags),
            0,
            HARDDOOM2_CMD_W3(cmd->pos_a_x, cmd->pos_a_y),
            HARDDOOM2_CMD_W3(cmd->pos_b_x, cmd->pos_b_y),
            0,
            0,
            HARDDOOM2_CMD_W6_A(0, 0, cmd->fill_color),
            0
        };
        return;
    }
    default: {
        /* TODO other cmds */
        dev_cmd->data = {
            HARDDOOM2_CMD_W0(HARDDOOM2_CMD_TYPE_FILL_RECT, extra_flags),
            0,
            HARDDOOM2_CMD_W3(0, 0),
            0,
            0,
            0,
            HARDDOOM2_CMD_W6_A(1, 1, 0),
            0
        };
    }
    }
}

static struct cmd make_setup(struct file** bufs, uint32_t extra_flags) {
    static const uint32_t bufs_flags[NUM_USER_BUFS] = {
        HARDDOOM2_CMD_FLAG_SETUP_SURF_DST, HARDDOOM2_CMD_FLAG_SETUP_SURF_SRC,
        HARDDOOM2_CMD_FLAG_SETUP_TEXTURE, HARDDOOM2_CMD_FLAG_SETUP_FLAT, HARDDOOM2_CMD_FLAG_SETUP_COLORMAP,
        HARDDOOM2_CMD_FLAG_SETUP_TRANSLATION, HARDDOOM2_CMD_FLAG_SETUP_TRANMAP };

    int i;
    for (i = 0; i < NUM_USER_BUFS; ++i) {
        if (bufs[i]) {
            extra_flags |= bufs_flags[i];
        }
    }

    uint16_t dst_width = 0;
    uint16_t src_width = 0;
    if (bufs[0]) {
        dst_width = ((struct buffer*)bufs[0]->private_data)->width;
    }
    if (bufs[1]) {
        src_width = ((struct buffer*)bufs[1]->private_data)->width;
    }

    struct cmd cmd;
    cmd.data[0] = HARDDOOM2_CMD_W0_SETUP(HARDDOOM2_CMD_TYPE_SETUP, extra_flags, dst_width, src_width);
    for (i = 0; i < NUM_USER_BUFFS; ++i) {
        cmd.data[i+1] = bufs[i] ? (((struct buffer*)bufs[i]->private_data)->page_table_dev >> 8) : 0;
    }

    return cmd;
}

static uint32_t get_cmd_buf_space(struct harddoom2* hd2) {
    return ioread32(hd2->bar + HARDDOOM2_CMD_READ_IDX) - hd2->write_idx - 1;
}

static void enable_intr(struct harddoom2* hd2, uint32_t intr) {
    /* TODO synchro */
    iowrite32(ioread32(hd2->bar + HARDDOOM2_INTR_ENABLE)  | intr, hd2->bar + HARDDOOM2_INTR_ENABLE);
}

static void disable_intr(struct harddoom2* hd2, uint32_t intr) {
    /* TODO synchro */
    iowrite32(ioread32(hd2->bar + HARDDOOM2_INTR_ENABLE)  & ~intr, hd2->bar + HARDDOOM2_INTR_ENABLE);
}

static void deactivate_intr(struct harddoom2* hd2, uint32_t intr) {
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

    int set = setup_buffers(hd2, bufs);
    if (set < 0) {
        DEBUG("write: could not setup");
        goto out_setup;
    } else if (set) {
        --space;
    }

    if (num_cmds > space) {
        num_cmds = space;
    }

    BUG_ON(!num_cmds);

    uint32_t write_idx = hd2->write_idx;
    uint32_t extra_flags = (write_idx % PING_PERIOD) ? 0 : HARDDOOM2_CMD_FLAG_PING_ASYNC;

    struct cmd dev_cmd;
    size_t it;
    for (it = 0; it < num_cmds - 1; ++it) {
        make_cmd(&dev_cmd, &cmds[it], extra_flags);
        write_cmd(hd2, &dev_cmd, write_idx);

        write_idx = (write_idx + 1) % CMD_BUF_SIZE;
        extra_flags = (write_idx % PING_PERIOD) ? 0 : HARDDOOM2_CMD_FLAG_PING_ASYNC;
    }

    /* it = num_cmds - 1 */
    extra_flags |= HARDDOOM2_CMD_FLAG_FENCE;
    make_cmd(&dev_cmd, &cmds[it], extra_flags);
    write_cmd(hd2, &dev_cmd, write_idx);

    hd2->write_idx = (write_idx + 1) % CMD_BUF_SIZE;
    iowrite32(hd2->write_idx, hd2->bar + HARDDOOM2_CMD_WRITE_IDX);
    cnt_incr(&hd2->batch_cnt);

    ((struct buffer*)hd2->curr_bufs[0]->private_data)->last_write = hd2->batch_cnt;

    /* TODO: update only buffers that were actually used */
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
