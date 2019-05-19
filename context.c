#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/uaccess.h>

#include "doomdev2.h"

#include "hd2.h"
#include "hd2_buffer.h"
#include "common.h"

#include "context.h"

#define MAX_KMALLOC (128 * 1024)

static int context_open(struct inode* inode, struct file* file);
static int context_release(struct inode* inode, struct file* file);
static long context_ioctl(struct file* file, unsigned cmd, unsigned long arg);
static ssize_t context_write(struct file* file, const char __user* _buf, size_t count, loff_t* off);

const struct file_operations _context_ops = {
    .owner = THIS_MODULE,
    .open = context_open,
    .release = context_release,
    .unlocked_ioctl = context_ioctl,
    .compat_ioctl = context_ioctl,
    .write = context_write
};

const struct file_operations* const context_ops = &_context_ops;

struct context {
    struct harddoom2* hd2;
    struct hd2_buffer* curr_bufs[NUM_USER_BUFS];
};

static struct context* get_ctx(struct file* file) {
    BUG_ON(!file);
    BUG_ON(!file->private_data);
    BUG_ON(file->f_op != context_ops);

    struct context* ctx = (struct context*)file->private_data;

    BUG_ON(!ctx->hd2);

    return ctx;
}

static int context_open(struct inode* inode, struct file* file) {
    int number = MINOR(inode->i_rdev);
    if (number < 0 || number >= 256) { ERROR("context_open: minor not in range"); return -EINVAL; }

    struct context* ctx = kzalloc(sizeof(struct context), GFP_KERNEL);
    if (!ctx) {
        DEBUG("ctx: kmalloc");
        return -ENOMEM;
    }

    ctx->hd2 = get_hd2(number);
    file->private_data = ctx;

    return 0;
}

static int context_release(struct inode* inode, struct file* file) {
    struct context* ctx = get_ctx(file);

    release_user_bufs(ctx->curr_bufs);

    kfree(ctx);
    return 0;
}

static int setup(struct context* ctx, struct doomdev2_ioctl_setup __user* _params) {
    struct doomdev2_ioctl_setup params;

    DEBUG("setup");

    if (copy_from_user(&params, _params, sizeof(struct doomdev2_ioctl_setup))) {
        DEBUG("setup copy_from_user fail");
        return -EFAULT;
    }

    int32_t fds[NUM_USER_BUFS] = { params.surf_dst_fd, params.surf_src_fd, params.texture_fd,
        params.flat_fd, params.colormap_fd, params.translation_fd, params.tranmap_fd };
    struct hd2_buffer* bufs[NUM_USER_BUFS] = { NULL };

    int err;
    int i;
    for (i = 0; i < NUM_USER_BUFS; ++i) {
        if (fds[i] == -1) continue;

        bufs[i] = hd2_buff_fd_get(fds[i]);
        if (IS_ERR(bufs[i])) {
            DEBUG("setup: wrong fd");
            err = PTR_ERR(bufs[i]);
            goto out_fds;
        }
    }

    int j;
    for (j = 0; j < 2; ++j) {
        if (!bufs[j]) continue;

        if (!is_surface(bufs[j])) {
            DEBUG("setup: non-surface given as surface");
            err = -EINVAL;
            goto out_fds;
        }
    }
    for (; j < NUM_USER_BUFS; ++j) {
        if (!bufs[j]) continue;

        if (is_surface(bufs[j])) {
            DEBUG("setup: surface given as non-surface");
            err = -EINVAL;
            goto out_fds;
        }
    }

    for (j = 0; j < NUM_USER_BUFS; ++j) {
        hd2_buff_put(ctx->curr_bufs[j]);
        ctx->curr_bufs[j] = bufs[j];
    }

    return 0;

out_fds:
    for (--i; i >= 0; --i) {
        hd2_buff_put(bufs[i]);
    }

    return err;
}

static long context_ioctl(struct file* file, unsigned cmd, unsigned long arg) {
    struct context* ctx = get_ctx(file);

    switch (cmd) {
    case DOOMDEV2_IOCTL_CREATE_SURFACE:
        return harddoom2_create_surface(ctx->hd2, (struct doomdev2_ioctl_create_surface __user*)arg);
    case DOOMDEV2_IOCTL_CREATE_BUFFER:
        return harddoom2_create_buffer(ctx->hd2, (struct doomdev2_ioctl_create_buffer __user*)arg);
    case DOOMDEV2_IOCTL_SETUP:
        return setup(ctx, (struct doomdev2_ioctl_setup __user*)arg);
    }

    return -ENOTTY;
}

static int validate_cmd(struct context* ctx, const struct doomdev2_cmd* user_cmd) {
    struct hd2_buffer* dst_buff = ctx->curr_bufs[0];
    uint16_t dst_width = get_buff_width(dst_buff), dst_height = get_buff_height(dst_buff);

    switch (user_cmd->type) {
    case DOOMDEV2_CMD_TYPE_FILL_RECT: {
        const struct doomdev2_cmd_fill_rect* cmd = &user_cmd->fill_rect;
        if ((uint32_t)cmd->pos_x + cmd->width > dst_width || (uint32_t)cmd->pos_y + cmd->height > dst_height) {
            DEBUG("fill_rect: out of bounds");
            return -EINVAL;
        }
        return 0;
    }
    case DOOMDEV2_CMD_TYPE_DRAW_LINE: {
        const struct doomdev2_cmd_draw_line* cmd = &user_cmd->draw_line;
        if (cmd->pos_a_x >= dst_width || cmd->pos_a_y >= dst_height
                || cmd->pos_b_x >= dst_width || cmd->pos_b_y >= dst_height) {
            DEBUG("draw_line: out of bounds");
            return -EINVAL;
        }
        return 0;
    }
    }

    /* TODO: other cmds */

    return 0;
}

static ssize_t context_write(struct file* file, const char __user* _buf, size_t count, loff_t* off) {
    _Static_assert(MAX_KMALLOC % sizeof(struct doomdev2_cmd) == 0, "cmd size mismatch");
    _Static_assert(MAX_KMALLOC <= LONG_MAX && sizeof(ssize_t) == sizeof(long), "ssize max");
    ssize_t err;

    struct context* ctx = get_ctx(file);

    if (!count || count % sizeof(struct doomdev2_cmd) != 0) {
        DEBUG("context_write: wrong count %lu", count);
        return -EINVAL;
    }

    if (count > MAX_KMALLOC) {
        count = MAX_KMALLOC;
    }

    if (!ctx->curr_bufs[0]) {
        DEBUG("write: no dst surface set");
        return -EINVAL;
    }

    void* buf = kmalloc(count, GFP_KERNEL);
    if (!buf) {
        DEBUG("context_write: kmalloc fail");
        return -ENOMEM;
    }

    if (copy_from_user(buf, _buf, count)) {
        DEBUG("context_write: copy from user fail");
        err = -EFAULT;
        goto out_copy;
    }

    size_t num_cmds = count / sizeof(struct doomdev2_cmd);
    struct doomdev2_cmd* cmds = (struct doomdev2_cmd*)buf;

    size_t it;
    for (it = 0; it < num_cmds; ++it) {
        if ((err = validate_cmd(ctx, &cmds[it]))) {
            break;
        }
    }

    if (!it) {
        /* The first command was invalid, return error */
        DEBUG("write: first command invalid");
        goto out_copy;
    }

    num_cmds = it;
    err = harddoom2_write(ctx->hd2, ctx->curr_bufs, cmds, num_cmds);
    BUG_ON(!err || err > num_cmds);
    if (err > 0) {
        err *= sizeof(struct doomdev2_cmd);
    }

out_copy:
    kfree(buf);
    return err;
}
