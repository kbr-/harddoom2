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
    struct mutex mut;
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
    unsigned number = MINOR(inode->i_rdev);
    BUG_ON(number >= 256);

    struct context* ctx = kzalloc(sizeof(struct context), GFP_KERNEL);
    if (!ctx) {
        DEBUG("ctx: kmalloc");
        return -ENOMEM;
    }

    ctx->hd2 = get_hd2(number);
    mutex_init(&ctx->mut);

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

    if (copy_from_user(&params, _params, sizeof(struct doomdev2_ioctl_setup))) {
        DEBUG("setup copy_from_user fail");
        return -EFAULT;
    }

    int32_t fds[NUM_USER_BUFS] = { params.surf_dst_fd, params.surf_src_fd, params.texture_fd,
        params.flat_fd, params.translation_fd, params.colormap_fd, params.tranmap_fd };
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

    err = -EINVAL;

    int j;
    for (j = 0; j < 2; ++j) {
        if (!bufs[j]) continue;

        if (!is_surface(bufs[j])) {
            DEBUG("setup: non-surface given as surface");
            goto out_fds;
        }
    }
    for (; j < NUM_USER_BUFS; ++j) {
        if (!bufs[j]) continue;

        BUG_ON(!get_buff_size(bufs[j]));
        if (is_surface(bufs[j])) {
            DEBUG("setup: surface given as non-surface");
            goto out_fds;
        }
    }
    if (bufs[DST_BUF_IDX] && bufs[SRC_BUF_IDX] &&
            (get_buff_width(bufs[DST_BUF_IDX]) != get_buff_width(bufs[SRC_BUF_IDX]) ||
             get_buff_height(bufs[DST_BUF_IDX]) != get_buff_height(bufs[SRC_BUF_IDX]))) {
        /* The only command that uses the the source buffer, COPY_RECT, assumes they have the same dimensions. */
        DEBUG("setup: different dst and src buf dims");
        goto out_fds;
    }
    if (bufs[FLAT_BUF_IDX] && get_buff_size(bufs[FLAT_BUF_IDX]) % (1 << 12)) {
        DEBUG("setup: flat buf wrong size");
        goto out_fds;
    }
    if (bufs[TRANSLATE_BUF_IDX] && get_buff_size(bufs[TRANSLATE_BUF_IDX]) % (1 << 8)) {
        DEBUG("setup: translate buf wrong size");
        goto out_fds;
    }
    if (bufs[COLORMAP_BUF_IDX] && get_buff_size(bufs[COLORMAP_BUF_IDX]) % (1 << 8)) {
        DEBUG("setup: colormap buf wrong size");
        goto out_fds;
    }
    if (bufs[TRANMAP_BUF_IDX] && get_buff_size(bufs[TRANMAP_BUF_IDX]) % (1 << 16)) {
        DEBUG("setup: tranmap buf wrong size");
        goto out_fds;
    }

    mutex_lock(&ctx->mut);
    for (j = 0; j < NUM_USER_BUFS; ++j) {
        hd2_buff_put(ctx->curr_bufs[j]);
        ctx->curr_bufs[j] = bufs[j];
    }
    mutex_unlock(&ctx->mut);

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

static bool validate_maps(struct context* ctx, uint8_t flags, uint16_t colormap_idx, uint16_t translation_idx) {
    if (flags & DOOMDEV2_CMD_FLAGS_TRANSLATE && !ctx->curr_bufs[TRANSLATE_BUF_IDX]) {
        DEBUG("draw_column: translate flag set but no buf");
        return false;
    }
    if (flags & DOOMDEV2_CMD_FLAGS_COLORMAP && !ctx->curr_bufs[COLORMAP_BUF_IDX]) {
        DEBUG("draw_column: colormap flag set but no buf");
        return false;
    }
    if (flags & DOOMDEV2_CMD_FLAGS_TRANMAP && !ctx->curr_bufs[TRANMAP_BUF_IDX]) {
        DEBUG("draw_column: tranmap flag set but no buf");
        return false;
    }
    if (flags & DOOMDEV2_CMD_FLAGS_TRANSLATE
            && translation_idx >= (get_buff_size(ctx->curr_bufs[TRANSLATE_BUF_IDX]) >> 8)) {
        DEBUG("draw_column: translation idx out of bounds");
        return false;
    }
    if (flags & DOOMDEV2_CMD_FLAGS_COLORMAP
            && colormap_idx >= (get_buff_size(ctx->curr_bufs[COLORMAP_BUF_IDX]) >> 8)) {
        DEBUG("draw_column: colormap idx out of bounds");
        return false;
    }
    return true;
}

static bool validate_cmd(struct context* ctx, const struct doomdev2_cmd* user_cmd) {
    BUG_ON(!ctx->curr_bufs[DST_BUF_IDX]);

    struct hd2_buffer* dst_buff = ctx->curr_bufs[DST_BUF_IDX];
    uint16_t surf_width = get_buff_width(dst_buff), surf_height = get_buff_height(dst_buff);
    {
        struct hd2_buffer* src_buff = ctx->curr_bufs[SRC_BUF_IDX];
        BUG_ON(src_buff && (get_buff_width(src_buff) != surf_width || get_buff_height(src_buff) != surf_height));
    }

    switch (user_cmd->type) {
    case DOOMDEV2_CMD_TYPE_COPY_RECT: {
        if (!ctx->curr_bufs[SRC_BUF_IDX]) {
            DEBUG("copy rect: no src buf");
            return false;
        }

        const struct doomdev2_cmd_copy_rect* cmd = &user_cmd->copy_rect;
        if ((uint32_t)cmd->pos_dst_x + cmd->width > surf_width ||
                (uint32_t)cmd->pos_dst_y + cmd->height > surf_height ||
                (uint32_t)cmd->pos_src_x + cmd->width > surf_width ||
                (uint32_t)cmd->pos_src_y + cmd->height > surf_height) {
            DEBUG("copy_rect: out of bounds");
            return false;
        }

        if (ctx->curr_bufs[DST_BUF_IDX] == ctx->curr_bufs[SRC_BUF_IDX] &&
               cmd->pos_dst_x < cmd->pos_src_x + cmd->width && cmd->pos_dst_x + cmd->width > cmd->pos_src_x &&
               cmd->pos_dst_y < cmd->pos_src_y + cmd->height && cmd->pos_dst_y + cmd->height > cmd->pos_src_y) {
            DEBUG("copy_rect: intersection");
            return false;
        }
        return true;
    }
    case DOOMDEV2_CMD_TYPE_FILL_RECT: {
        const struct doomdev2_cmd_fill_rect* cmd = &user_cmd->fill_rect;
        if ((uint32_t)cmd->pos_x + cmd->width > surf_width || (uint32_t)cmd->pos_y + cmd->height > surf_height) {
            DEBUG("fill_rect: out of bounds");
            return false;
        }
        return true;
    }
    case DOOMDEV2_CMD_TYPE_DRAW_LINE: {
        const struct doomdev2_cmd_draw_line* cmd = &user_cmd->draw_line;
        if (cmd->pos_a_x >= surf_width || cmd->pos_a_y >= surf_height
                || cmd->pos_b_x >= surf_width || cmd->pos_b_y >= surf_height) {
            DEBUG("draw_line: out of bounds");
            return false;
        }
        return true;
    }
    case DOOMDEV2_CMD_TYPE_DRAW_BACKGROUND: {
        if (!ctx->curr_bufs[FLAT_BUF_IDX]) {
            DEBUG("draw background: no flat buf");
            return false;
        }

        const struct doomdev2_cmd_draw_background* cmd = &user_cmd->draw_background;
        if ((uint32_t)cmd->pos_x + cmd->width > surf_width || (uint32_t)cmd->pos_y + cmd->height > surf_height) {
            DEBUG("draw background: out of bounds");
            return false;
        }
        if (cmd->flat_idx >= (get_buff_size(ctx->curr_bufs[FLAT_BUF_IDX]) >> 12)) {
            DEBUG("draw background: flat idx out of bounds");
            return false;
        }
        return true;
    }
    case DOOMDEV2_CMD_TYPE_DRAW_COLUMN: {
        if (!ctx->curr_bufs[TEXTURE_BUF_IDX]) {
            DEBUG("draw_column: no texture buffer");
            return false;
        }

        const struct doomdev2_cmd_draw_column* cmd = &user_cmd->draw_column;
        if (cmd->pos_x >= surf_width || cmd->pos_b_y >= surf_height) {
            DEBUG("draw_column: out of bounds");
            return false;
        }
        if (cmd->pos_b_y < cmd->pos_a_y) {
            DEBUG("draw_column: b_y < a_y");
            return false;
        }
        if (!validate_maps(ctx, cmd->flags, cmd->colormap_idx, cmd->translation_idx)) {
            return false;
        }
        return true;
    }
    case DOOMDEV2_CMD_TYPE_DRAW_SPAN: {
        if (!ctx->curr_bufs[FLAT_BUF_IDX]) {
            DEBUG("draw span: no flat buffer");
            return false;
        }

        const struct doomdev2_cmd_draw_span* cmd = &user_cmd->draw_span;
        if (cmd->pos_y >= surf_height || cmd->pos_b_x >= surf_width) {
            DEBUG("draw span: out of bounds");
            return false;
        }
        if (cmd->pos_b_x < cmd->pos_a_x) {
            DEBUG("draw span: b_x < a_x");
            return false;
        }
        if (cmd->flat_idx >= (get_buff_size(ctx->curr_bufs[FLAT_BUF_IDX]) >> 12)) {
            DEBUG("draw span: flat idx out of bounds");
            return false;
        }
        if (!validate_maps(ctx, cmd->flags, cmd->colormap_idx, cmd->translation_idx)) {
            return false;
        }
        return true;
    }
    case DOOMDEV2_CMD_TYPE_DRAW_FUZZ: {
        if (!ctx->curr_bufs[COLORMAP_BUF_IDX]) {
            DEBUG("draw fuzz: no colormap");
            return false;
        }

        const struct doomdev2_cmd_draw_fuzz* cmd = &user_cmd->draw_fuzz;
        if (cmd->pos_x >= surf_width || cmd->fuzz_end >= surf_height) {
            DEBUG("draw fuzz: out of bounds");
            return false;
        }
        if (cmd->fuzz_start > cmd->pos_a_y || cmd->pos_a_y > cmd->pos_b_y || cmd->pos_b_y > cmd->fuzz_end) {
            DEBUG("draw fuzz: inequalities");
            return false;
        }
        if (cmd->colormap_idx >= (get_buff_size(ctx->curr_bufs[COLORMAP_BUF_IDX]) >> 8)) {
            DEBUG("draw fuzz: colormap idx out of bounds");
            return false;
        }
        if (cmd->fuzz_pos > 55) {
            DEBUG("draw fuzz: fuzz_pos > 55");
            return false;
        }
        return true;
    }
    }

    DEBUG("unknown cmd type: %u", user_cmd->type);
    return false;
}

static ssize_t context_write(struct file* file, const char __user* _buf, size_t count, loff_t* off) {
    _Static_assert(MAX_KMALLOC % sizeof(struct doomdev2_cmd) == 0, "cmd size mismatch");
    _Static_assert(MAX_KMALLOC <= LONG_MAX && sizeof(ssize_t) == sizeof(long), "ssize max");
    ssize_t err;

    if (!count || count % sizeof(struct doomdev2_cmd) != 0) {
        DEBUG("context_write: wrong count %lu", count);
        return -EINVAL;
    }

    if (count > MAX_KMALLOC) {
        count = MAX_KMALLOC;
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

    struct context* ctx = get_ctx(file);

    mutex_lock(&ctx->mut);
    if (!ctx->curr_bufs[DST_BUF_IDX]) {
        DEBUG("write: no dst surface set");
        err = -EINVAL;
        goto out_surf;
    }

    size_t it;
    for (it = 0; it < num_cmds; ++it) {
        if (!validate_cmd(ctx, &cmds[it])) {
            err = -EINVAL;
            break;
        }
    }

    if (!it) {
        /* The first command was invalid, return error */
        DEBUG("write: first command invalid");
        goto out_surf;
    }

    num_cmds = it;
    err = harddoom2_write(ctx->hd2, ctx->curr_bufs, cmds, num_cmds);
    BUG_ON(!err || err > num_cmds);
    if (err > 0) {
        err *= sizeof(struct doomdev2_cmd);
    }

out_surf:
    mutex_unlock(&ctx->mut);
out_copy:
    kfree(buf);
    return err;
}
