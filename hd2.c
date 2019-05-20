#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/uaccess.h>
#include <linux/cdev.h>
#include <linux/pci.h>

#include "doomcode2.h"
#include "harddoom2.h"

#include "common.h"
#include "context.h"
#include "dma_buffer.h"
#include "hd2.h"

MODULE_LICENSE("GPL");

#define CHRDEV_PREFIX "doom"
#define DEVICES_LIMIT 256
#define PING_PERIOD 2048
#define HARDDOOM2_ADDR_SIZE 40
#define NUM_INTR_BITS 15

/* 32 */
#define CMD_SEND_BYTES (HARDDOOM2_CMD_SEND_SIZE * sizeof(uint32_t))

/* 4M */
#define MAX_BUFFER_SIZE (MAX_BUFFER_PAGES * HARDDOOM2_PAGE_SIZE)

/* 128K commands */
#define CMD_BUF_LEN (MAX_BUFFER_SIZE / CMD_SEND_BYTES)

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

    struct dma_buffer cmd_buff;

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
    struct hd2_buffer* curr_bufs[NUM_USER_BUFS];
};

struct buffer_change {
    /* NULL pointer represents no change. */
    /* Non-NULL pointer indicates what buffer was used BEFORE the change. */
    struct hd2_buffer* bufs[NUM_USER_BUFS];

    /* Number of the batch in which this set of buffers was removed */
    struct counter cnt;

    struct list_head list;
};

static struct harddoom2 devices[DEVICES_LIMIT];

struct harddoom2* get_hd2(unsigned num) {
    BUG_ON(num >= DEVICES_LIMIT);
    return &devices[num];
}

static int dev_counter = 0;
// static DEFINE_MUTEX(dev_counter_mut);

static int alloc_dev_number(void) {
    int ret = -ENOSPC;

    /* TODO lock */
    if (dev_counter < DEVICES_LIMIT) {
        ret = dev_counter++;
    }
    return ret;
}

typedef void (*doom_irq_handler_t)(struct harddoom2*, uint32_t);

static void handle_fence(struct harddoom2* hd2, uint32_t bit) {
    DEBUG("handle fence");

    wake_up_all(&hd2->fence_wq);
}

static void handle_pong_async(struct harddoom2* hd2, uint32_t bit) {
    DEBUG("pong_async");

    wake_up_all(&hd2->write_wq);
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
        HARDDOOM2_INTR_PAGE_FAULT_SURF_SRC, HARDDOOM2_INTR_PAGE_FAULT_TEXTURE, HARDDOOM2_INTR_PAGE_FAULT_FLAT,
        HARDDOOM2_INTR_PAGE_FAULT_TRANSLATION, HARDDOOM2_INTR_PAGE_FAULT_COLORMAP, HARDDOOM2_INTR_PAGE_FAULT_TRANMAP };

    _Static_assert(NUM_INTR_BITS == ARRAY_SIZE(intr_bits), "num intr bits");

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
    iowrite32(0, bar + HARDDOOM2_FE_CODE_ADDR);
    for (int i = 0; i < ARRAY_SIZE(doomcode2); ++i) {
        iowrite32(doomcode2[i], bar + HARDDOOM2_FE_CODE_WINDOW);
    }
    iowrite32(HARDDOOM2_RESET_ALL, bar + HARDDOOM2_RESET);

    iowrite32(cmds_page_table >> 8, bar + HARDDOOM2_CMD_PT);
    iowrite32(CMD_BUF_LEN, bar + HARDDOOM2_CMD_SIZE);
    iowrite32(0, bar + HARDDOOM2_CMD_READ_IDX);
    iowrite32(0, bar + HARDDOOM2_CMD_WRITE_IDX);

    iowrite32(HARDDOOM2_INTR_MASK, bar + HARDDOOM2_INTR);
    iowrite32(HARDDOOM2_INTR_MASK & ~HARDDOOM2_INTR_PONG_ASYNC, bar + HARDDOOM2_INTR_ENABLE);

    iowrite32(0, bar + HARDDOOM2_FENCE_COUNTER);

    iowrite32(HARDDOOM2_ENABLE_ALL, bar + HARDDOOM2_ENABLE);
}

static void device_off(void __iomem* bar) {
    iowrite32(0, bar + HARDDOOM2_ENABLE);
    iowrite32(0, bar + HARDDOOM2_INTR_ENABLE);
    ioread32(bar + HARDDOOM2_ENABLE);
}

/* Make sure to call device_off before freeing the buffers. */
static void free_buffers(struct harddoom2* hd2) {
    free_dma_buff(&hd2->cmd_buff);

    release_user_bufs(hd2->curr_bufs);

    while (!list_empty(&hd2->changes_queue)) {
        struct buffer_change* change = list_first_entry(&hd2->changes_queue, struct buffer_change, list);

        release_user_bufs(change->bufs);

        list_del(&change->list);
        kfree(change);
    }
}

static dev_t doom_major;

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
        DEBUG("can't set dma mask, err: %d", err);
        err = -EIO;
        goto out_dma;
    }
    if ((err = pci_set_consistent_dma_mask(pdev, DMA_BIT_MASK(HARDDOOM2_ADDR_SIZE)))) {
        DEBUG("can't set consistent dma mask, err: %d", err);
        err = -EIO;
        goto out_dma;
    }

    /* TODO: free dev numbers, use list? */
    if ((err = alloc_dev_number()) < 0) {
        DEBUG("can't allocate device number");
        goto out_dma;
    }

    int dev_number = err;
    BUG_ON(dev_number >= DEVICES_LIMIT);

    struct harddoom2* hd2 = &devices[dev_number];

    memset(hd2, 0, sizeof(struct harddoom2));
    hd2->number = dev_number;
    hd2->bar = bar;
    hd2->pdev = pdev;

    if ((err = init_dma_buff(&hd2->cmd_buff, MAX_BUFFER_SIZE, &pdev->dev))) {
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

    reset_device(bar, hd2->cmd_buff.page_table_dev);

    cdev_init(&hd2->cdev, context_ops);
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
    return 0;
}

static void pci_shutdown(struct pci_dev* dev) {
    DEBUG("shutdown");
    /* TODO cleanup */
}

static struct pci_driver pci_drv = {
    .name = DRV_NAME,
    .id_table = pci_ids,
    .probe = pci_probe,
    .remove = pci_remove,
    .suspend = pci_suspend,
    .resume = pci_resume,
    .shutdown = pci_shutdown
};

static int hd2_init(void)
{
    int err;

	DEBUG("Init");
    if ((err = alloc_chrdev_region(&doom_major, 0, DEVICES_LIMIT, DRV_NAME))) {
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

_Static_assert(sizeof(struct cmd) == 32, "struct cmd size");

static struct cmd make_cmd(const struct doomdev2_cmd* user_cmd, uint32_t extra_flags) {
    switch (user_cmd->type) {
    case DOOMDEV2_CMD_TYPE_FILL_RECT: {
        const struct doomdev2_cmd_fill_rect* cmd = &user_cmd->fill_rect;
        return (struct cmd){ .data = {
            HARDDOOM2_CMD_W0(HARDDOOM2_CMD_TYPE_FILL_RECT, extra_flags),
            0,
            HARDDOOM2_CMD_W3(cmd->pos_x, cmd->pos_y),
            0,
            0,
            0,
            HARDDOOM2_CMD_W6_A(cmd->width, cmd->height, cmd->fill_color),
            0
        }};
    }
    case DOOMDEV2_CMD_TYPE_DRAW_LINE: {
        const struct doomdev2_cmd_draw_line* cmd = &user_cmd->draw_line;
        return (struct cmd){ .data = {
            HARDDOOM2_CMD_W0(HARDDOOM2_CMD_TYPE_DRAW_LINE, extra_flags),
            0,
            HARDDOOM2_CMD_W3(cmd->pos_a_x, cmd->pos_a_y),
            HARDDOOM2_CMD_W3(cmd->pos_b_x, cmd->pos_b_y),
            0,
            0,
            HARDDOOM2_CMD_W6_A(0, 0, cmd->fill_color),
            0
        }};
    }
    case DOOMDEV2_CMD_TYPE_DRAW_COLUMN: {
        const struct doomdev2_cmd_draw_column* cmd = &user_cmd->draw_column;
        if (cmd->flags & DOOMDEV2_CMD_FLAGS_TRANSLATE) extra_flags |= HARDDOOM2_CMD_FLAG_TRANSLATION;
        if (cmd->flags & DOOMDEV2_CMD_FLAGS_COLORMAP) extra_flags |= HARDDOOM2_CMD_FLAG_COLORMAP;
        if (cmd->flags & DOOMDEV2_CMD_FLAGS_TRANMAP) extra_flags |= HARDDOOM2_CMD_FLAG_TRANMAP;
        return (struct cmd){ .data = {
            HARDDOOM2_CMD_W0(HARDDOOM2_CMD_TYPE_DRAW_COLUMN, extra_flags),
            HARDDOOM2_CMD_W1(
                cmd->flags & DOOMDEV2_CMD_FLAGS_TRANSLATE ? cmd->translation_idx : 0,
                cmd->colormap_idx & DOOMDEV2_CMD_FLAGS_COLORMAP ? cmd->colormap_idx : 0),
            HARDDOOM2_CMD_W3(cmd->pos_x, cmd->pos_a_y),
            HARDDOOM2_CMD_W3(cmd->pos_x, cmd->pos_b_y),
            cmd->ustart,
            cmd->ustep,
            HARDDOOM2_CMD_W6_B(cmd->texture_offset),
            HARDDOOM2_CMD_W7_B((get_buff_size(hd2->curr_bufs[TEXTURE_BUF_IDX]) - 1) >> 6, cmd->texture_height)
        }};
    }
    }

    /* TODO other cmds */

    return (struct cmd){ .data = {
        HARDDOOM2_CMD_W0(HARDDOOM2_CMD_TYPE_FILL_RECT, extra_flags),
        0,
        HARDDOOM2_CMD_W3(0, 0),
        0,
        0,
        0,
        HARDDOOM2_CMD_W6_A(1, 1, 0),
        0
    }};
}

static struct cmd make_setup(struct hd2_buffer* bufs[NUM_USER_BUFS], uint32_t extra_flags) {
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

    uint16_t dst_width = bufs[0] ? get_buff_width(bufs[0]) : 0;
    uint16_t src_width = bufs[1] ? get_buff_width(bufs[1]) : 0;

    struct cmd cmd;
    cmd.data[0] = HARDDOOM2_CMD_W0_SETUP(HARDDOOM2_CMD_TYPE_SETUP, extra_flags, dst_width, src_width);
    for (i = 0; i < NUM_USER_BUFS; ++i) {
        cmd.data[i+1] = bufs[i] ? (get_page_table(bufs[i]) >> 8) : 0;
    }

    return cmd;
}

static void write_cmd(struct harddoom2* hd2, struct cmd* cmd, size_t write_idx) {
    struct dma_buffer* buff = &hd2->cmd_buff;
    BUG_ON(write_idx >= CMD_BUF_LEN);
    BUG_ON(buff->size != CMD_BUF_LEN * CMD_SEND_BYTES);

    size_t dst_pos = write_idx * CMD_SEND_BYTES;
    return write_dma_buff(buff, cmd->data, dst_pos, CMD_SEND_BYTES);
}

static int setup_buffers(struct harddoom2* hd2, struct hd2_buffer* bufs[NUM_USER_BUFS]) {
    int has_change = 0;
    int is_diff = 0;

    int i;
    for (i = 0; i < NUM_USER_BUFS; ++i) {
        if (bufs[i] != hd2->curr_bufs[i]) {
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
        if (bufs[i] == hd2->curr_bufs[i]) continue;

        if (bufs[i]) {
            hd2_buff_get(bufs[i]);
        }
        if (hd2->curr_bufs[i]) {
            BUG_ON(!change);
            change->bufs[i] = hd2->curr_bufs[i];
        }
        hd2->curr_bufs[i] = bufs[i];
    }

    struct cmd dev_cmd = make_setup(hd2->curr_bufs,
            (hd2->write_idx % PING_PERIOD) ? 0 : HARDDOOM2_CMD_FLAG_PING_ASYNC);
    write_cmd(hd2, &dev_cmd, hd2->write_idx);
    hd2->write_idx = (hd2->write_idx + 1) % CMD_BUF_LEN;

    return 1;
}

static void _update_last_fence_cnt(struct harddoom2* hd2) {
    uint32_t curr_lower = ioread32(hd2->bar + HARDDOOM2_FENCE_COUNTER);
    uint32_t last_lower = cnt_lower(hd2->last_fence_cnt);

    uint32_t upper = cnt_upper(hd2->last_fence_cnt);
    if (last_lower > curr_lower) {
        ++upper;
    }
    hd2->last_fence_cnt = make_cnt(upper, curr_lower);
}

void bump_fence_wait(struct harddoom2* hd2, struct counter cnt) {
    /* TODO spinlock */
    if (!cnt_ge(cnt, hd2->last_fence_wait)) {
        goto out;
    }

    iowrite32(cnt_lower(cnt), hd2->bar + HARDDOOM2_FENCE_WAIT);
    hd2->last_fence_wait = cnt;

out:
    /* TODO spinunlock */
    return;
}

struct counter get_curr_fence_cnt(struct harddoom2* hd2) {
    struct counter res;
    /* TODO spinlock */
    _update_last_fence_cnt(hd2);
    res = hd2->last_fence_cnt;
    /* TODO spinunlock */
    return res;
}

void wait_for_fence_cnt(struct harddoom2* hd2, struct counter cnt) {
    if (cnt_ge(get_curr_fence_cnt(hd2), cnt)) {
        return;
    }

    bump_fence_wait(hd2, cnt);
    if (cnt_ge(get_curr_fence_cnt(hd2), cnt)) {
        return;
    }

    DEBUG("wait for fence: %llu", cnt.val);

    wait_event(hd2->fence_wq, cnt_ge(get_curr_fence_cnt(hd2), cnt));

    DEBUG("wait for fence: %llu finished", cnt.val);
}

void update_last_fence_cnt(struct harddoom2* hd2) {
    /* TODO spinlock */
    _update_last_fence_cnt(hd2);
}

static void collect_buffers(struct harddoom2* hd2) {
    struct counter cnt = get_curr_fence_cnt(hd2);
    while (!list_empty(&hd2->changes_queue)) {
        struct buffer_change* change = list_first_entry(&hd2->changes_queue, struct buffer_change, list);

        if (!cnt_ge(cnt, change->cnt)) {
            return;
        }

        release_user_bufs(change->bufs);

        list_del(&change->list);
        kfree(change);
    }
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
    iowrite32(intr, hd2->bar + HARDDOOM2_INTR);
}

ssize_t harddoom2_write(struct harddoom2* hd2, struct hd2_buffer* bufs[NUM_USER_BUFS],
        const struct doomdev2_cmd* cmds, size_t num_cmds) {
    update_last_fence_cnt(hd2);

    /* TODO mutex lock */
    while (get_cmd_buf_space(hd2) < 2) {
        deactivate_intr(hd2, HARDDOOM2_INTR_PONG_ASYNC);
        if (get_cmd_buf_space(hd2) >= 2) {
            break;
        }
        enable_intr(hd2, HARDDOOM2_INTR_PONG_ASYNC);
        /* TODO unlock */

        wait_event(hd2->write_wq, get_cmd_buf_space(hd2) >= 2);

        /* TODO mutex lock */
    }

    disable_intr(hd2, HARDDOOM2_INTR_PONG_ASYNC);
    /* If anyone was waiting on the event queue, let them enable the interrupt again. */
    wake_up_all(&hd2->write_wq);

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

    for (size_t it = 0; it < num_cmds - 1; ++it) {
        struct cmd dev_cmd = make_cmd(&cmds[it], extra_flags);
        write_cmd(hd2, &dev_cmd, write_idx);

        write_idx = (write_idx + 1) % CMD_BUF_LEN;
        extra_flags = (write_idx % PING_PERIOD) ? 0 : HARDDOOM2_CMD_FLAG_PING_ASYNC;
    }

    extra_flags |= HARDDOOM2_CMD_FLAG_FENCE;
    struct cmd dev_cmd = make_cmd(&cmds[num_cmds - 1], extra_flags);
    write_cmd(hd2, &dev_cmd, write_idx);

    hd2->write_idx = (write_idx + 1) % CMD_BUF_LEN;
    iowrite32(hd2->write_idx, hd2->bar + HARDDOOM2_CMD_WRITE_IDX);
    cnt_incr(&hd2->batch_cnt);

    set_last_write(hd2->curr_bufs[0], hd2->batch_cnt);

    /* TODO: update only buffers that were actually used */
    int i;
    for (i = 0; i < NUM_USER_BUFS; ++i) {
        if(hd2->curr_bufs[i]) {
            set_last_use(hd2->curr_bufs[i], hd2->batch_cnt);
        }
    }

    /* TODO do this outside lock? */
    collect_buffers(hd2);

    /* TODO unlock */

    return num_cmds;

out_setup:
    /* TODO unlock */
    return set;
}

int harddoom2_create_surface(struct harddoom2* hd2, struct doomdev2_ioctl_create_surface __user* _params) {
    DEBUG("harddoom2 create surface");

    struct doomdev2_ioctl_create_surface params;
    if (copy_from_user(&params, _params, sizeof(struct doomdev2_ioctl_create_surface))) {
        DEBUG("create surface copy_from_user fail");
        return -EFAULT;
    }

    DEBUG("create_surface: %u, %u", (unsigned)params.width, (unsigned)params.height);
    if (params.width < 64 || !params.height || params.width % 64) {
        return -EINVAL;
    }

    if (params.width > 2048 || params.height > 2048) {
        return -EOVERFLOW;
    }

    size_t size = params.width * params.height;
    if (size > MAX_BUFFER_SIZE) { ERROR("create surface size"); return -EINVAL; }

    return new_hd2_buffer(hd2, size, params.width, params.height);
}

int harddoom2_create_buffer(struct harddoom2* hd2, struct doomdev2_ioctl_create_buffer __user* _params) {
    DEBUG("harddoom2 create buffer");

    struct doomdev2_ioctl_create_buffer params;
    if (copy_from_user(&params, _params, sizeof(struct doomdev2_ioctl_create_buffer))) {
        DEBUG("create_buffer copy_from_user fail");
        return -EFAULT;
    }

    DEBUG("create_buffer: %u", params.size);
    if (!params.size) {
        return -EINVAL;
    }
    if (params.size > MAX_BUFFER_SIZE) {
        return -EOVERFLOW;
    }

    return new_hd2_buffer(hd2, params.size, 0, 0);
}

int harddoom2_init_dma_buff(struct harddoom2* hd2, struct dma_buffer* buff, size_t size) {
    return init_dma_buff(buff, size, &hd2->pdev->dev);
}
