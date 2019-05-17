static int context_open(struct inode* inode, struct file* file);
static int context_release(struct inode* inode, struct file* file);
static int context_ioctl(struct file* file, unsigned cmd, unsigned long arg);
static ssize_t context_write(struct file* file, const char __user* _buf, size_t count, loff_t* off);

struct file_operations context_ops = {
    .owner = THIS_MODULE,
    .open = context_open,
    .release = context_release,
    .unlocked_ioctl = context_ioctl,
    .compat_ioctl = context_ioctl,
    .write = context_write
};

struct context {
    struct harddoom2* hd2;
    struct fd curr_bufs[NUM_USER_BUFS];
};

static struct context* get_ctx(struct file* file) {
    if (!file) { ERROR("get_ctx: file NULL"); return ERR_PTR(-EINVAL); }
    if (!file->private_data) { ERROR("get_ctx: file private data NULL"); return ERR_PTR(-EINVAL); }
    if (file->f_ops != doom_ops) { ERROR("get_ctx: wrong fops"); return ERR_PTR(-EINVAL); }

    struct context* ctx = (struct context*)file->private_data;

    if (!ctx->hd2) { ERROR("get_ctx no hd2!"); return ERR_PTR(-EINVAL); }
    if (!ctx->hd2->cmd_buff) { ERROR("get_ctx no cmd_buff!"); return ERR_PTR(-EINVAL); }

    return ctx;
}

static int context_open(struct inode* inode, struct file* file) {
    int number = MINOR(inode->i_rdev);
    if (number < 0 || number >= DEVICES_LIMIT) {
        ERROR("context_open: minor not in range");
        return -EINVAL;
    }

    struct context* ctx = kzalloc(sizeof(struct context), GFP_KERNEL);
    if (!ctx) {
        return -ENOMEM;
    }

    ctx->hd2 = &devices[number];
    file->private_data = ctx;

    return 0;
}

static int context_release(struct inode* inode, struct file* file) {
    struct context* ctx = get_ctx(file);
    if (IS_ERR(ctx)) { ERROR("context_ioctl"); return PTR_ERR(ctx); }

    int i;
    for (i = 0; i < NUM_USER_BUFS; ++i) {
        fdput(ctx->curr_bufs[i]);
    }

    kfree(ctx);
    return 0;
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

static int context_ioctl(struct file* file, unsigned cmd, unsigned long arg) {
    struct context* ctx = get_ctx(file);
    if (IS_ERR(ctx)) { ERROR("context_ioctl"); return PTR_ERR(ctx); }

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
    struct buffer* buff = (struct buffer*)ctx->curr_bufs[0].file->private_data;
    uint16_t dst_width = buff->width, dst_height = buff->height;

    switch (user_cmd->type) {
    case DOOMDEV2_CMD_TYPE_FILL_RECT: {
        struct doomdev2_cmd_fill_rect* cmd = &user_cmd->fill_rect;
        if ((uint32_t)cmd->pos_x + cmd->width > dst_width || (uint32_t)cmd->pos_y + cmd.height > dst_height) {
            DEBUG("fill_rect: out of bounds");
            return -EINVAL;
        }
        return 0;
    }
    case DOOMDEV2_CMD_TYPE_DRAW_LINE: {
        struct doomdev2_cmd_draw_line* cmd = &user_cmd->draw_line;
        if (cmd->pos_a_x >= dst_width || cmd->pos_a_y >= dst_height
                || cmd->pos_b_x >= dst_width || cmd->pos_b_y >= dst_height) {
            DEBUG("draw_line: out of bounds");
            return -EINVAL;
        }
        return 0;
    }
    default:
        /* TODO: other cmds */
    }

    return 0;
}

static ssize_t context_write(struct file* file, const char __user* _buf, size_t count, loff_t* off) {
    _Static_assert(MAX_KMALLOC % sizeof(struct doomdev2_cmd) == 0, "cmd size mismatch");
    _Static_assert(MAX_KMALLOC >= SSIZE_MAX);
    ssize_t err;

    struct context* ctx = get_ctx(file);
    if (IS_ERR(ctx)) { ERROR("context_write ctx"); return PTR_ERR(ctx); }

    if (!count || count % sizeof(doomdev2_cmd) != 0) {
        DEBUG("context_write: wrong count %llu", count);
        return -EINVAL;
    }

    if (count > MAX_KMALLOC) {
        count = MAX_KMALLOC;
    }

    if (!ctx->curr_bufs[0].file) {
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
        if ((err = validate_cmd(cmds[it]))) {
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
    if (err >= 0) {
        err *= sizeof(struct doomdev2_cmd);
    }

out_copy:
    kfree(buf);
    return err;
}
