#include "buffer.h"

static int buffer_release(struct inode* inode, struct file* file);
static ssize_t buffer_write(struct file* file, const char __user* _buff, size_t count, loff_t* off);
static ssize_t buffer_read(struct file* file, char __user *_buff, size_t count, loff_t* off);
static loff_t buffer_llseek(struct file* file, loff_t off, int whence);

struct file_operations buffer_ops = {
    .owner = THIS_MODULE,
    // .open = buffer_open,
    .release = buffer_release,
    .write = buffer_write,
    .read = buffer_read,
    .llseek = buffer_llseek
};

int make_buffer(struct harddoom2* hd2, size_t size, uint16_t width, uint16_t height) {
    int err;

    struct buffer* buff = kmalloc(sizeof(struct buffer), GFP_KERNEL);
    if (!buff) {
        DEBUG("make_buffer: kmalloc");
        return -ENOMEM;
    }

    if ((err = init_buffer(buff, hd2, size))) {
        DEBUG("make_buffer: init_buffer");
        goto out_buff;
    }

    buff->width = width;
    buff->height = height;

    int flags = O_RDWR | O_CREAT;

    int fd = get_unused_fd_flags(flags);
    if (fd < 0) {
        DEBUG("make_buffer: get unused fd, %d", fd);
        err = fd;
        goto out_getfd;
    }

    struct file* f = anon_inode_getfile(CHRDEV_NAME, buffer_ops, buff, flags);
    if (IS_ERR(f)) {
        DEBUG("make_buffer: getfile");
        err = PTR_ERR(f);
        goto out_getfile;
    }

    f->f_mode = FMODE_LSEEK | FMODE_PREAD | FMORE_PWRITE;

    fd_install(fd, f);
    /* TODO: need to do anything else? */

    return fd;

out_getfile:
    put_unused_fd(fd);
out_getfd:
    free_buffer(buff);
out_buff:
    kfree(buff);
    return err;
}

// static int buffer_open(struct inode* inode, struct file* file) {
//     /* We don't allow opening a buffer multiple times by the user. */
//     return -ENODEV;
// }

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

    if (!count) {
        return -EINVAL;
    }

    wait_for_fence_cnt(&buff->hd2, buff->last_use);
    /* Someone might have moved buff->last_use forward by now, but we don't care.
       It's the user's responsibility not to send any more commands using this buffer
       in the middle of a buffer_write or buffer_read call. */

    size_t ret = 0;
    int err = write_buffer_user(buff, _buff, *off, count, &ret);
    *off += ret;

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

    wait_for_fence_cnt(&buff->hd2, buff->last_write);
    /* See comment in buffer_write. */

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
