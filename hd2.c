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
#define NUM_USER_BUFS 7
#define PING_PERIOD (2048 + 32)

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

struct device_data {
    int number;
    void __iomem* bar;
    struct cdev cdev;
    struct buffer cmd_buff;

    struct pci_dev* pdev;

    /* Used to wait for free space in the command buffer. */
    wait_queue_head_t cmd_wait_queue;

    /* Manages the lifetime of buffers used by the device.
       When a SETUP command is sent to the command queue, we remember the set of changed buffers
       in the queue, increasing their reference counts. We periodically clear the queue,
       removing buffers which have been replaced and decreasing their reference counts. */
    struct list_head changes_queue;

    /* Buffers used in the last SETUP command sent to the command buffer. */
    struct file* last_bufs[NUM_USER_BUFS];
};

struct buffer_change {
    /* NULL pointer represents no change. */
    /* Non-NULL pointer indicates what buffer was used BEFORE the change. */
    struct file* bufs[NUM_USER_BUFS];
    uint32_t cmd_number;

    struct list_head list;
}

static struct device_data devices[DEVICES_LIMIT];

struct context {
    /* TODO? */
    struct device_data* devdata;

    struct fd curr_bufs[NUM_USER_BUFS];
};

struct context* get_ctx(struct file* file) {
    if (!file) { ERROR("get_ctx: file NULL"); return ERR_PTR(-EINVAL); }
    if (!file->private_data) { ERROR("get_ctx: file private data NULL"); return ERR_PTR(-EINVAL); }
    if (file->f_ops != doom_ops) { ERROR("get_ctx: wrong fops"); return ERR_PTR(-EINVAL); }

    return (struct context*)file->private_data;
}

static int buffer_open(struct inode* inode, struct file* file) {
    /* We don't allow opening a buffer multiple times by the user. */
    return -ENODEV;
}

static int buffer_release(struct inode* inode, struct file* file) {
    struct buffer* buff = (struct buffer*)file->private_data;
    if (!buff) { ERROR("buffer_release"); return -EINVAL; }
    free_buffer(buff);
    kfree(buff);
    file->private_data = NULL;

    return 0;
}

static ssize_t buffer_write(struct file* file, const char __user* _buff, size_t count, loff_t* off) {
    struct buffer* buff = file->private_data;
    if (!buff) { ERROR("buffer write: no buff!"); return -EINVAL; }
    if (file->f_op != buffer_ops) { ERROR("buffer write: wrong fops"); return -EINVAL; }

    if (*off < 0) {
        return -EINVAL;
    }

    if (*off >= buff->size) {
        return -ENOSPC;
    }

    if (count > buff->size - *off) {
        count = buff_size - *off;
    }

    /* TODO: wait for ops */

    if (!count) {
        return -EINVAL;
    }

    size_t ret = 0;
    int err = write_buffer_user(buff, _buff, *off, count, &ret);
    *off ++ ret;

    if (err && !ret) {
        return err;
    }

    if (!ret) { ERROR("buffer_write: OOPS, !ret"); return -EINVAL; }

    return ret;
}

static ssize_t buffer_read(struct file* file, char __user *_buff, size_t count, loff_t* off) {
    struct buffer* buff = file->private_data;
    if (!buff) { ERROR("buffer read: no buff!"); return -EINVAL; }
    if (file->f_op != buffer_ops) { ERROR("buffer read: wrong fops"); return -EINVAL; }

    if (*off < 0) {
        return -EINVAL;
    }

    if (*off >= buff->size) {
        return 0;
    }

    if (count > buff->size - *off) {
        count = buff_size - *off;
    }

    if (!count) {
        return -EINVAL;
    }

    /* TODO: wait for ops */

    size_t ret = 0;
    int err = read_buffer_user(buff, _buff, *off, count, &ret);
    *off += ret;

    if (err && !ret) {
        return err;
    }

    if (!ret) { ERROR("buffer_read: OOPS, !ret"); return -EINVAL; }

    return ret;
}

static loff_t buffer_llseek(struct file* file, loff_t off, int whence) {
    struct buffer* buff = file->private_data;
    if (!buff) { ERROR("llseek: no buff!"); return -EINVAL; }
    if (file->f_op != buffer_ops) { ERROR("llseek: wrong fops"); return -EINVAL; }
    if (file->f_pos < 0 || file->f_pos >= buff->size) { ERROR("OOPS: wrong fpos!"); return -EINVAL; }

    if (whence == SEEK_CUR) {
        off += file->f_pos;
    } else if (whence == SEEK_END) {
        off += buff->size;
    } else if (whence != SEEK_SET) {
        DEBUG("llseek: wrong whence");
        return -EINVAL;
    }

    if (off < 0 || off >= buff->size) {
        DEBUG("llseek: SEEK_SET: out of bounds");
        return -EINVAL;
    }

    file->f_pos = off;
    return off;
}

static struct file_operations buffer_ops = {
    .owner = THIS_MODULE,
    .open = buffer_open,
    .release = buffer_release,
    .write = buffer_write,
    .read = buffer_read,
    .llseek = buffer_llseek
};

struct pci_dev* get_pci_dev(struct context* ctx) {
    /* TODO might be gone? */
    return ctx->pdev;
}

int make_buffer(struct context* ctx, const char* class, size_t size, uint16_t width, uint16_t height) {
    /* TODO: is this class needed? */
    int err;

    struct buffer* buff = kmalloc(sizeof(struct buffer), GFP_KERNEL);
    if (!buff) {
        DEBUG("make_buffer: kmalloc");
        return -ENOMEM;
    }

    struct pci_dev* pdev = get_pci_dev(ctx);
    if ((err = init_buffer(buff, &pdev->dev, size))) {
        DEBUG("make_buffer: init_buffer");
        goto out_buff;
    }

    buff->width = width;
    buff->height = height;

    int fd = anon_inode_getfd(class, buffer_ops, buff, O_RDWR | O_CREAT);
    if (fd < 0) {
        DEBUG("make_buffer: failed to get fd, %d", fd);
        err = fd;
        goto out_getfd;
    }

    struct fd f = fdget(fd);
    /* TODO: better use anon_inode_getfile, set f_mode, then install fd... ? */
    f.file->f_mode = FMODE_LSEEK | FMODE_PREAD | FMORE_PWRITE;
    f.file->private_data = buff;
    /* TODO: need to do anything else? */
    fdput(f);

    return fd;

out_getfd:
    free_buff(buff);
out_buff:
    kfree(buff);
    return err;
}

static int create_surface(struct context* ctx, struct doomdev2_ioctl_create_surface __user* _params) {
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

    return make_buffer(ctx, "SURFACE", params,width * params.height, params.width, params.height);
}

static int create_buffer(struct context* ctx, struct doomdev2_ioctl_create_buffer __user* _params) {
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

    return make_buffer(ctx, "BUFFER", params.size, 0, 0);
}

static int wait_for_cmd_buffer(struct context* ctx, uint32_t write_idx, uint32_t* free_cmds) {
    /* TODO synchronization */
    void __iomem* bar = ctx->devdata->bar;

    *free_cmds = ioread32(bar + HARDDOOM2_CMD_READ_IDX) - write_idx - 1;
    if (*free_cmds >= CMD_BUF_SIZE) { ERROR("number of free cmds: %d", free_cmds); return -EINVAL; }

    if (*free_cmds >= 2) {
        return 0;
    }

    DEBUG("doom_write: no space in queue");

    iowrite32(HARDDOOM2_INTR_PONG_ASYNC, bar + HARDDOOM2_INTR);

    /* TODO: why? */
    *free_cmds = ioread32(bar + HARDDOOM2_CMD_READ_IDX) - write_idx - 1;
    if (*free_cmds >= CMD_BUF_SIZE) { ERROR("number of free cmds: %d", free_cmds); return -EINVAL; }
    if (*free_cmds >= 2) {
        return 0;
    }

    uint32_t intrs = ioread32(bar + HARDDOOM2_INTR_ENABLE);
    intrs |= HARDDOOM2_INTR_PONG_ASYNC;
    iowrite(intrs, bar + HARDDOOM2_INTR_ENABLE);

    /* TODO interruptible: check if something broke */
    wait_event(&ctx->cmd_wait_queue,
        (*free_cmds = ioread32(bar + HARDDOOM2_CMD_READ_IDX) - *write_idx) >= 2);

    intrs = ioread32(bar + HARDDOOM2_INTR_ENABLE);
    intrs &= ~HARDDOOM2_INTR_PONG_ASYNC;
    iowrite(intrs, bar + HARDDOOM2_INTR_ENABLE);

    return 0;
}

static struct cmd make_setup(struct context* ctx, uint32_t extra_flags) {
    static const uint32_t bufs_flags[NUM_USER_BUFS] = {
        HARDDOOM2_CMD_FLAG_SETUP_SURF_DST, HARDDOOM2_CMD_FLAG_SETUP_SURF_SRC,
        HARDDOOM2_CMD_FLAG_SETUP_TEXTURE, HARDDOOM2_CMD_FLAG_SETUP_FLAT, HARDDOOM2_CMD_FLAG_SETUP_COLORMAP,
        HARDDOOM2_CMD_FLAG_SETUP_TRANSLATION, HARDDOOM2_CMD_FLAG_SETUP_TRANMAP };

    int i;
    for (i = 0; i < NUM_USER_BUFS; ++i) {
        if (ctx->curr_bufs[i].file) {
            extra_flags |= bufs_flags[i];
        }
    }

    uint16_t dst_width = 0;
    uint16_t src_width = 0;
    {
        struct file* f;
        if ((f = ctx->curr_bufs[0].file)) {
            struct buffer* buff = (struct buffer*)f->private_data;
            dst_width = buff->width;
        }
        if ((f = ctx->curr_bufs[1].file)) {
            struct buffer* buff = (struct buffer*)f->private_data;
            src_width = buff->width;
        }
    }

    struct cmd hd_cmd;
    hd_cmd.data[0] = HARDDOOM2_CMD_W0_SETUP(HARDDOOM2_CMD_TYPE_SETUP, extra_flags, dst_width, src_width);
    for (int i = 0; i < NUM_USER_BUFFS; ++i) {
        struct file* f = ctx->curr_bufs[i].file;
        hd_cmd.data[i+1] = f ? (((struct buffer*)f->private_data)->page_table_dev >> 8) : 0;
    }

    return hd_cmd;
}

static int setup(struct context* ctx, struct doomdev2_ioctl_setup __user* _params) {
    struct doomdev_2_ioctl_setup params;

    DEBUG("setup");

    if (copy_from_user(&params, _params, sizeof(struct doomdev2_ioctl_setup))) {
        DEBUG("setup copy_from_user fail");
        return -EFAULT;
    }

    int32_t fds[NUM_USER_BUFS] = { params.surf_dst_fd, params.surf_src_fd, params.texture_fd,
        params.flat_fd, params.colormap_fd, params.translation_fd, params.tranmap_fd };
    struct fd fs[NUM_USER_BUFS];

    int err;
    int i;
    for (i = 0; i < NUM_USER_BUFS; ++i) {
        if (fds[i] == -1) {
            fs[i] = (struct fd){NULL, 0};
            continue;
        }

        fs[i] = fdget(fds[i]);
        if (!fs[i].file) {
            DEBUG("setup: fd no file");
            err = -EINVAL;
            goto out_files;
        }
        if (fs[i].file->f_op != buffer_ops) {
            DEBUG("setup: fd wrong file");
            err = -EINVAL;
            goto out_f_op;
        }
    }

    int j;
    for (j = 0; j < 2; ++j) {
        if (fds[j] == -1) continue;

        struct buffer* buff = (struct buffer*)fs[j].file->private_data;
        if (!buff->width) {
            DEBUG("setup: non-surface given as surface");
            err = -EINVAL;
            goto out_f_op;
        }
    }
    for (; j < NUM_USER_BUFS; ++j) {
        if (fds[j] == -1) continue;

        struct buffer* buff = (struct buffer*)fs[j].file->private_data;
        if (buff->width) {
            DEBUG("setup: surface given as non-surface");
            err = -EINVAL;
            goto out_f_op;
        }
    }

    for (j = 0; j < NUM_USER_BUFS; ++j) {
        fdput(ctx->curr_bufs[j]);
        ctx->curr_bufs[j] = fs[j];
    }

    return 0;

out_f_op:
    fdput(fs[i]);
out_files:
    for (--i; i >= 0; --i) {
        fdput(fs[i]);
    }

    return err;
}

static int doom_open(struct inode* inode, struct file* file) {
    int number = MINOR(inode->i_rdev);
    if (number < 0 || number >= DEVICES_LIMIT) {
        ERROR("doom_open: minor not in range");
        return -EINVAL;
    }

    struct context* ctx = kzalloc(sizeof(struct context), GFP_KERNEL);
    if (!ctx) {
        return -ENOMEM;
    }

    ctx->devdata = &devices[number];
    file->private_data = ctx;

    return 0;
}

static int doom_release(struct inode* inode, struct file* file) {
    struct context* ctx = get_ctx(file);
    if (IS_ERR(ctx)) { ERROR("doom_ioctl"); return PTR_ERR(ctx); }

    int i;
    for (i = 0; i < NUM_USER_BUFS; ++i) {
        fdput(ctx->curr_bufs[i]);
    }

    kfree(ctx);
    return 0;
}

static int doom_ioctl(struct file* file, unsigned cmd, unsigned long arg) {
    struct context* ctx = get_ctx(file);
    if (IS_ERR(ctx)) { ERROR("doom_ioctl"); return PTR_ERR(ctx); }

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

static int make_cmd(struct context* ctx, const struct doomdev2_cmd* user_cmd, struct cmd* dev_cmd, uint32_t extra_flags) {
    if (!ctx->curr_bufs[0].file) {
        DEBUG("write: no dst surface set");
        return -EINVAL;
    }

    struct buffer* buff = (struct buffer*)ctx->curr_bufs[0].file->private_data;
    uint16_t dst_width = buff->width, dst_height = buff->height;

    switch (user_cmd->type) {
    case DOOMDEV2_CMD_TYPE_FILL_RECT: {
        struct doomdev2_cmd_fill_rect cmd = user_cmd->fill_rect;
        uint32_t pos_x = cmd.pos_x, pos_y = cmd.pos_y;

        if (pos_x + cmd.width > dst_width || pos_y + cmd.height > dst_height) {
            DEBUG("fill_rect: out of bounds");
            return -EINVAL;
        }

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

        return 0;
    }
    case DOOMDEV2_CMD_TYPE_DRAW_LINE: {
        struct doomdev2_cmd_draw_line cmd = user_cmd->draw_line;
        if (cmd.pos_a_x >= dst_width || cmd.pos_a_y >= dst_height
                || cmd.pos_b_x >= dst_width || cmd.pos_b_y >= dst_height) {
            DEBUG("draw_line: out of bounds");
            return -EINVAL;
        }

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

        return 0;
    }
    default:
        /* TODO other cmds */
        return 1;
    }
}

static int write_cmd(struct context* ctx, struct cmd* cmd, size_t write_idx) {
    if (write_idx >= CMD_BUF_SIZE) { ERROR("write_cmd wrong write_idx! %llu", write_idx); return -EINVAL; }
    if (!ctx->devdata) { ERROR("write_cmd no device!"); return -EINVAL; }
    if (!ctx->devdata->cmd_buff) { ERROR("write_cmd no cmd_buff!"); return -EINVAL; }

    struct buffer* buff = &ctx->devdata->cmd_buff;
    if (buff->size != CMD_BUF_SIZE) { ERROR("wrong cmd_buff size?!"); return -EINVAL; }

    size_t dst_pos = write_idx * 8;
    return write_buffer(buff, cmd->data, dst_pos, 8*4);
}

/* Checks whether 'x' is in the range ['start', ..., 'end'] (mod 2^32). */
static bool in_range(uint32_t x, uint32_t start, uint32_t end) {
    if (start <= end) {
        return start <= x && x <= end;
    }

    return start <= x || x <= end;
}

static void collect_buffers(struct device_data* dev) {
    uint32_t counter = ioread32(dev->bar + HARDDOOM2_FENCE_COUNTER);
    while (!list_empty(dev->changes_queue)) {
        struct buffer_change* change = list_first_entry(&dev->changes_queue, struct buffer_change, list);

        /* If counter is in the range [cmd_number, ..., cmd_number + 3 * CMD_BUF_SIZE] (mod 2^32),
           then cmd_number is not in the range [counter + 1, ..., counter + CMD_BUF_SIZE + 2048 (mod 2^32)],
           so this SETUP was already processed by the device and we can collect previous buffers.

           If counter is not in the range [cmd_number, ..., cmd_number + 3 * CMD_BUF_SIZE],
           this SETUP couldn't have been processed yet: if it had, at least one
           collect_buffers would have been called while counter was still in this range,
           so the command would not be in the queue. */
        if (!in_range(counter, change->cmd_number, change->cmd_number + 3 * CMD_BUF_SIZE)) {
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

static int ensure_setup(struct context* ctx, size_t write_idx, uint32_t extra_flags) {
    struct file** last_bufs = ctx->devdata->last_bufs;
    struct fd* curr_bufs = ctx->curr_bufs;

    int has_change = 0;
    int is_diff = 0;

    int i;
    for (i = 0; i < NUM_USER_BUFS; ++i) {
        if (curr_bufs[i].file != last_bufs[i]) {
            is_diff = 1;
            if (last_bufs[i]) {
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
        list_add_tail(&change->list, &ctx->devdata->changes_queue);
    }

    struct cmd cmd = make_setup(ctx, extra_flags);
    int err;
    if ((err = write_cmd(ctx, &cmd, write_idx))) /* TODO: this should not fail */ {
        if (change) {
            list_del(&change_list);
            kfree(change);
            return err;
        }
    }

    for (i = 0; i < NUM_USER_BUFS; ++i) {
        if (curr_bufs[i].file == last_bufs[i]) continue;

        if (curr_bufs[i].file) {
            fget(curr_bufs[i].file);
        }
        if (last_bufs[i]) {
            change->bufs[i] = last_bufs[i];
        }
        last_bufs[i] = curr_bufs[i].file;
    }

    return 0;
}

static ssize_t doom_write(struct file* file, const char __user* _buf, size_t count, loff_t* off) {
    _Static_assert(MAX_KMALLOC % sizeof(struct doomdev2_cmd) == 0, "cmd size mismatch");
    int err;

    struct context* ctx = get_context(file);
    if (IS_ERR(ctx)) { ERROR("doom_write ctx"); return PTR_ERR(ctx); }

    if (!count || count % sizeof(doomdev2_cmd) != 0) {
        DEBUG("doom_write: wrong count %llu", count);
        return -EINVAL;
    }

    if (count > MAX_KMALLOC) {
        count = MAX_KMALLOC;
    }

    void* buf = kmalloc(count, GFP_KERNEL);
    if (!buf) {
        DEBUG("doom_write: kmalloc fail");
        return -ENOMEM;
    }

    if (copy_from_user(buf, _buf, count)) {
        DEBUG("doom_write: copy from user fail");
        err = -EFAULT;
        goto out_copy;
    }

    size_t num_cmds = count / sizeof(doomdev2_cmd);
    struct doomdev2_cmd* cmds = (struct doomdev2_cmd*)buf;

    struct cmd dev_cmd;
    if ((err = make_cmd(ctx, &cmds[0], &dev_cmd, 0)) < 0) /* TODO */ {
        /* The first command was invalid, return error */
        DEBUG("write: first command invalid");
        goto out_copy;
    }

    uint32_t write_idx = ioread32(bar + HARDDOOM2_CMD_WRITE_IDX);

    uint32_t free_cmds;
    /* TODO wait_for_cmd_buffer should never fail */
    if ((err = wait_for_cmd_buffer(ctx, write_idx, &free_cmds))) {
        goto out_copy;
    }

    /* TODO: free_cmds > 1 */

    uint32_t extra_flags = (write_idx % PING_PERIOD) ? 0 : HARDDOOM2_CMD_FLAG_PING_ASYNC;

    int set = ensure_setup(ctx, HARDDOOM2_CMD_FLAG_FENCE | extra_flags);
    if (set < 0) {
        DEBUG("write: could not setup");
        goto out_user;
    } else if (set) {
        --free_cmds;
        write_idx = (write_idx + 1) % CMD_BUF_SIZE;
        extra_flags = (write_idx % PING_PERIOD) ? 0 : HARDDOOM2_CMD_FLAG_PING_ASYNC;
    }
    /* TODO: <= 1 fence */

    if (num_cmds > free_cmds) {
        num_cmds = free_cmds;
    }

    /* TODO: write_cmd should never fail */

    size_t it;
    for (it = 0; it < num_cmds; ++it) {
        /* We remake the first cmd to take extra_flags into account */
        if ((err = make_cmd(ctx, &cmds[it], &dev_cmd, extra_flags)) < 0) /* TODO */ {
            break;
        }

        if (!err) /* TODO */ {
            if ((err = write_cmd(ctx, &dev_cmd, write_idx))) {
                break;
            }
            write_idx = (write_idx + 1) % CMD_BUF_SIZE;
            extra_flags = (write_idx % PING_PERIOD) ? 0 : HARDDOOM2_CMD_FLAG_PING_ASYNC;
        }
    }

    iowrite32(write_idx, ctx->devdata->bar + HARDDOOM2_CMD_WRITE_IDX);

    /* TODO: if fence? */
    collect_buffers(ctx->devdata);

    kfree(buf);
    return it * sizeof(struct doomdev2_cmd); /* TODO: can this overflow? */

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

static int pci_probe(struct pci_dev* pdev, const struct pci_device_id* id) {
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

    data = &devices[dev_number];
    pci_set_drvdata(pdev, data);

    memset(data, 0, sizeof(device_data));
    data->number = dev_number;
    data->bar = bar;
    cdev_init(&data->cdev, &doom_ops);
    data->cdev.owner = THIS_MODULE;
    data->pdev = pdev;

    init_waitqueue_head(&data->cmd_wait_queue);
    INIT_LIST_HEAD(&data->changes_queue);

    /* TODO: setup device before cdev_add */

    if ((err = cdev_add(&data->cdev, doom_major + dev_number, 1))) {
        DEBUG("can't register cdev");
        goto out_cdev_add;
    }

    device = device_create(&doom_class, &pdev->dev, doom_major + dev_number, 0, CHRDEV_NAME "%d", dev_number);
    if (IS_ERR(device)) {
        DEBUG("can't create device");
        err = PTR_ERR(device);
        goto err_device;
    }

    if ((err = request_irq(pdev->irq, doom_irq_handler, IRQF_SHARED, CHRDEV_NAME, data))) {
        DEBUG("can't setup interrupt handler");
        goto err_irq;
    }

    return 0;

err_irq:
    device_destroy(&doom_class, doom_major + dev_number);
err_device:
    cdev_del(&data->cdev);
out_cdev_add:
    pci_set_drvdata(pdev, NULL);
    /* TODO */
out_dma:
    pci_clear_master(pdev);
    pci_iounmap(pdev, data->bar);
out_iomap:
    pci_release_regions(pdev);
out_regions:
    pci_disable_device(pdev);
out_enable:
    return err;
}

static void pci_remove(struct pci_dev* dev) {
    /* TODO collect changes */
    struct device_data* data = pci_get_drvdata(dev);
    void __iomem* bar = NULL;

    if (!data) {
        ERROR("pci_remove: no pcidata!");
    } else {
        free_irq(dev->irq, data);
        if (data->number >= 0 && data->number < DEVICES_LIMIT) {
            device_destroy(&doom_class, doom_major + data->number);
        } else {
            ERROR("pci_remove: OOPS");
            if (&devices[data->number] != data) {
                ERROR("pci_remove: double OOPS");
            }
        }
        data->number = -1;
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
