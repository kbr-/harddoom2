#include <linux/module.h>
#include <linux/kernel.h>
#include "harddoom2.h"

MODULE_LICENSE("GPL");

#define DEBUG(fmt, ...) printk(KERN_INFO "hd2: " fmt "\n", ##__VA_ARGS__)
#define ERROR(fmt, ...) printk(KERN_ERR "hd2: " fmt "\n", ##__VA_ARGS__)
#define DRV_NANE "harddoom2_driver"
#define CHRDEV_NAME "doom"
#define DEVICES_LIMIT 256
#define MAX_KMALLOC (128 * 1024)
#define PAGE_SIZE (4 * 1024)
#define MAX_BUFFER_SIZE (4 * 1024 * 1024)

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

struct buffer {
    void* pages_kern[1024];
    dma_addr_t pages_dev[1024];

    void* page_table_kern;
    dma_addr_t page_table_dev;

    size_t size;
};

int init_buffer(struct buffer* buff, struct device* dev, size_t size) {
}

int write_buffer(struct buffer* buff, void* src, size_t dst_pos, size_t size) {
    if (dst_pos >= buff->size || size >= buff->size || dst_pos + size >= buff->size) { ERROR("bad write_buffer!"); return -EINVAL; }

    size_t page = dst_pos / PAGE_SIZE;
    size_t page_off = dst_pos % PAGE_SIZE;
    size_t space_in_page = PAGE_SIZE - page_off;
    void* dst = pages_kern[page] + page_off;

    while (size) {
        if (space_in_page > size) {
            space_in_page = size;
        }

        memcpy(dst, src, space_in_page);
        src += space_in_page;
        size -= space_in_page;

        dst = pages_kern[++page];
        space_in_page = PAGE_SIZE;
    }

    return 0;
}

struct device_data {
    int number;
    void __iomem* bar;
    struct cdev cdev;
    struct buffer cmd_buff;
};

static struct device_data devices[DEVICES_LIMIT];

struct surface {
    uint16_t width;
    uint16_t height;
};

struct context {
    /* TODO */
    struct device_data* devdata;
    struct surface dst_surface;
};

struct context* get_ctx(struct file* file) {
    if (!file->private_data) { ERROR("OOPS, file private data NULL"); return -EFAULT; }

    return (struct context*)file->private_data;
}

static struct file_operations buffer_ops = {
    /* TODO */
};

static int create_surface(struct context* ctx, struct doomdev2_ioctl_create_surface __user* _params) {
    int fd;
    struct doomdev2_ioctl_create_surface params;
    struct fd sfd;

    if (copy_from_user(&params, _params, sizeof(struct doomdev2_ioctl_create_surface))) {
        DEBUG("create surface copy_from_user fail");
        return -EFAULT;
    }

    DEBUG("create_surface: %u, %u", (unsigned)params.width, (unsigned)params.height);
    if (params.width < 64 || params.width > 2048 || params.width % 64 != 0
            || params.height < 1 || params.height > 2048) {
        return -EINVAL;
    }

    /* TODO alloc buffer */
    void* buff;

    fd = anon_inode_getfd("SURFACE", buffer_ops, buff, O_RDWR | O_CREAT);
    if (fd < 0) {
        DEBUG("create surface: failed to get fd, %d" fd);
        goto out_getfd;
    }

    sfd = fdget(fd);
    /* TODO: need to do anything else? */
    sfd.file->f_mode = FMODE_LSEEK | FMODE_PREAD | FMORE_PWRITE;

    return fd;

out_getfd:
    /* TODO free buffer */
    return fd;
}

static int create_buffer(struct context* ctx, struct doomdev2_ioctl_create_buffer __user* _params) {
}

static int setup(struct context* ctx, struct doomdev2_ioctl_setup __user* _params) {
    /* TODO setup dst surface params if needed */
}

static int doom_open(struct inode* inode, struct file* file) {
    /* TODO */
    /* TODO allocate context, get device_data */
}

static int doom_release(struct inode* inode, struct file* file) {
    /* TODO */
}

static int doom_ioctl(struct file* file, unsigned cmd, unsigned long arg) {
    /* TODO */
    switch (cmd) {
    case DOOMDEV2_IOCTL_CREATE_SURFACE:
        return create_surface(ctx, (struct doomdev2_ioctl_create_surface __user*)arg);
    case DOOMDEV2_IOCTL_CREATE_BUFFER:
        return create_buffer(ctx, (struct doomdev2_ioctl_create_buffer __user*)arg);
    case DOOMDEV2_IOCTL_SETUP:
        return setup(ctx, (struct doomdev2_ioctl_setup __user*)arg);
    }

    return -ENOTTY;
}

struct cmd {
    uint32_t data[8];
};

int write_cmd(struct buffer* cmd_buff, struct cmd* cmd, size_t write_idx) {
    if (write_idx >= CMD_BUF_SIZE) { ERROR("write_cmd wrong write_idx! %llu", write_idx); return -EINVAL; }
    if (cmd_buff->size != CMD_BUF_SIZE) { ERROR("wrong cmd_buff size?!"); return -EINVAL; }

    size_t dst_pos = write_idx * 8;
    return write_buffer(cmd_buff, cmd->data, dst_pos, 8*4);
}

static ssize_t doom_write(struct file* file, const char __user* _buf, size_t count, loff_t* off) {
    _Static_assert(sizeof(struct doomdev2_cmd) % MAX_KMALLOC == 0, "cmd size mismatch");

    /* TODO */
    struct doomdev2_cmd* cmds;
    struct context* ctx;
    struct hd_buffer* cmd_buf;
    size_t num_cmds, it;
    int free_cmds;
    char* buf;
    int err;

    ctx = get_context(file);
    if (IS_ERR(ctx)) { ERROR("doom_write ctx"); return PTR_ERR(ctx); }
    cmd_buf = ctx->cmd_buf;

    if (count % sizeof(doomdev2_cmd) != 0) {
        DEBUG("doom_write: wrong count %llu", count);
        return -EINVAL;
    }

    if (count > MAX_KMALLOC) {
        count = MAX_KMALLOC;
    }

    buf = kmalloc(count, GFP_KERNEL);
    if (!buf) {
        DEBUG("doom_write: kmalloc fail");
        return -ENOMEM;
    }

    if (copy_from_user(buf, _buf, count)) {
        DEBUG("doom_write: copy from user fail");
        err = -EFAULT;
        goto out_copy;
    }

    num_cmds = count / sizeof(doomdev2_cmd);
    cmds = (struct doomdev2_cmd*)buf;

    /* TODO synchronization */
    write_idx = ioread32(ctx->devdata->bar + HARDDOOM2_CMD_WRITE_IDX);
    read_idx = ioread32(ctx->devdata->bar + HARDDOOM2_CMD_READ_IDX);
    free_cmds = read_idx - write_idx - 1; /* TODO is this correct? */
    if (free_cmds < 0) free_cmds += CMD_BUF_SIZE;
    if (free_cmds < 0 || free_cmds >= CMD_BUF_SIZE) { ERROR("number of free cmds: %d", free_cmds); err = -EFAULT; goto out_copy; }

    if (free_cmds == 0) {
        /* TODO wait instead */
        DEBUG("doom_write: no space in queue");
        err = -EAGAIN;
        goto out_copy;
    }

    if (num_cmds > free_cmds) {
        num_cmds = free_cmds;
    }

    err = 0;
    for (it = 0; it < num_cmds; ++it) {
        switch (cmds[it].type) {
        case DOOMDEV2_CMD_TYPE_FILL_RECT: {
            struct doomdev2_cmd_fill_rect cmd = cmds[it].fill_rect;
            uint32_t pos_x = cmd.pos_x;
            uint32_t pos_y = cmd.pos_y;
            if (pos_x + cmd.width >= ctx->dst_surface.width || pos_y + cmd.height >= ctx->dst_surface.height) {
                err = -EINVAL;
                break;
            }
            struct cmd hdcmd = {
                .data = {
                    HARDDOOM2_CMD_TYPE_FILL_RECT,
                    0,
                    HARDDOOM2_CMD_W3(cmd.pos_x, cmd.pos_y),
                    0,
                    0,
                    0,
                    HARDDOOM2_CMD_W6_A(cmd.width, cmd.height, cmd.fill_color),
                    0
                }
            };
            if ((err = write_cmd(&ctx->devdata->cmd_buff, &hd_cmd, write_idx))) return err;
            break;
        }
        case DOOMDEV2_CMD_TYPE_DRAW_LINE: {
            struct doomdev2_cmd_draw_line cmd = cmds[it].draw_line;
            if (cmd.pos_a_x >= ctx->dst_surface.width || cmd.pos_a_y >= ctx->dst_surface.height
                    || cmd.pos_b_x >= ctx->dst_surface.width || cmd.pos_b_y >= ctx->dst_surface.height) {
                err = -EINVAL;
                break;
            }
            struct cmd hdcmd {
                .data = {
                    HARDDOOM2_CMD_TYPE_DRAW_LINE,
                    0,
                    HARDDOOM2_CMD_W3(cmd.pos_a_x, cmd.pos_a_y),
                    HARDDOOM2_CMD_W3(cmd.pos_b_x, cmd.pos_b_y),
                    0,
                    0,
                    HARDDOOM2_CMD_W6_A(0, 0, cmd.fill_color),
                    0
                }
            };
            if ((err = write_cmd(&ctx->devdata->cmd_buff, &hd_cmd, write_idx))) return err;
            break;
        }
        default:
            /* TODO */
            break;
        }

        if (err) break;
        write_idx = (write_idx + 1) % CMD_BUF_SIZE;
    }

    if (it == 0) {
        /* TODO something failed */
        goto out_copy;
    }

    iowrite32(write_idx, ctx->devdata->bar + HARDDOOM2_CMD_WRITE_IDX);

    kfree(buf);
    return it * sizeof(struct doomdev2_cmd);

out_copy:
    kfree(buf);
    return err;
}

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

irqreturn_t doom_irq_handler(int irq, void* dev) {
    /* TODO */
    struct device_data* data = dev;
    if (!dev) {
        ERROR("No data in irq_handler!");
        BUG();
    }
}

static int pci_probe(struct pci_dev* dev, const struct pci_device_id* id) {
    int err = 0;
    void __iomem bar* = NULL;
    struct device* device = NULL;
    struct device_data* data = NULL;
    int dev_number = DEVICE_LIMIT;

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

    if ((err = request_irq(dev->irq, doom_irq_handler, IRQF_SHARED, CHRDEV_NAME, data))) {
        DEBUG("can't setup interrupt handler");
        goto err_irq;
    }

    return 0;

err_irq:
    device_destroy(&doom_class, doom_major + dev_number);
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
        free_irq(dev->irq, data);
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
