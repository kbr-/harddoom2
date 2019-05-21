#include <asm/bug.h>
#include <linux/fs.h>
#include <linux/file.h>
#include <linux/slab.h>
#include <linux/anon_inodes.h>

#include "common.h"
#include "dma_buffer.h"
#include "hd2.h"

#include "hd2_buffer.h"

struct hd2_buffer {
    struct dma_buffer dma_buff;

    struct harddoom2* hd2;

    /* An 'opened file' representing this buffer. Invariant: this->f->private_data == this */
    struct file* f;

    /* When was the buffer last written to/read from by the device? */
    struct counter last_use;
    struct counter last_write;

    /* Non-zero values indicate that this is a surface buffer.
       Zero indicates any other type of buffer (cmd, texture, etc.). */
    uint16_t width;
    uint16_t height;
};

static int hd2_buff_release(struct inode* inode, struct file* file) {
    DEBUG("hd2_buff_release");

    struct hd2_buffer* buff = file->private_data;
    BUG_ON(buff->f != file);

    free_dma_buff(&buff->dma_buff);
    kfree(buff);

    return 0;
}

static ssize_t hd2_buff_write(struct file* file, const char __user* _buff, size_t count, loff_t* off) {
    struct hd2_buffer* buff = file->private_data;
    BUG_ON(buff->f != file);

    if (*off < 0) {
        return -EINVAL;
    }

    if (*off >= buff->dma_buff.size) {
        return -ENOSPC;
    }

    if (count > buff->dma_buff.size - *off) {
        count = buff->dma_buff.size - *off;
    }

    if (!count) {
        return -EINVAL;
    }

    /* TODO mutex lock */
    struct counter last_use = buff->last_use;
    /* unlock */

    wait_for_fence_cnt(buff->hd2, last_use);
    /* Someone might have moved buff->last_use forward by now, but we don't care.
       It's the user's responsibility not to send any more commands using this buffer
       in the middle of a buffer_write or buffer_read call. */

    ssize_t ret = write_dma_buff_user(&buff->dma_buff, _buff, *off, count);
    if (ret < 0) {
        DEBUG("hd2 buff write: error");
        return ret;
    }
    *off += ret;

    BUG_ON(!ret);
    return ret;
}

static ssize_t hd2_buff_read(struct file* file, char __user *_buff, size_t count, loff_t* off) {
    struct hd2_buffer* buff = file->private_data;
    BUG_ON(buff->f != file);

    if (*off < 0) {
        return -EINVAL;
    }

    if (*off >= buff->dma_buff.size) {
        return 0;
    }

    if (count > buff->dma_buff.size - *off) {
        count = buff->dma_buff.size - *off;
    }

    if (!count) {
        return -EINVAL;
    }

    /* TODO mutex lock */
    struct counter last_write = buff->last_write;
    /* unlock */

    wait_for_fence_cnt(buff->hd2, last_write);
    /* See comment in buffer_write. */

    ssize_t ret = read_dma_buff_user(&buff->dma_buff, _buff, *off, count);
    if (ret < 0) {
        DEBUG("hd2_buff_read: read error");
        return ret;
    }
    *off += ret;

    BUG_ON(!ret);
    return ret;
}

static loff_t hd2_buff_llseek(struct file* file, loff_t off, int whence) {
    struct hd2_buffer* buff = file->private_data;
    BUG_ON(buff->f != file);
    BUG_ON(file->f_pos < 0 || file->f_pos > buff->dma_buff.size);

    if (whence == SEEK_CUR) {
        off += file->f_pos;
    } else if (whence == SEEK_END) {
        off += buff->dma_buff.size;
    } else if (whence != SEEK_SET) {
        DEBUG("llseek: wrong whence");
        return -EINVAL;
    }

    if (off < 0 || off > buff->dma_buff.size) {
        DEBUG("llseek: SEEK_SET: out of bounds");
        return -EINVAL;
    }

    file->f_pos = off;
    return off;
}

static const struct file_operations hd2_buff_ops = {
    .owner = THIS_MODULE,
    .release = hd2_buff_release,
    .write = hd2_buff_write,
    .read = hd2_buff_read,
    .llseek = hd2_buff_llseek
};

int new_hd2_buffer(struct harddoom2* hd2, size_t size, uint16_t width, uint16_t height) {
    BUG_ON((width && size != width * height) || size > MAX_BUFFER_PAGES * HARDDOOM2_PAGE_SIZE);
    int err;

    struct hd2_buffer* buff = kzalloc(sizeof(struct hd2_buffer), GFP_KERNEL);
    if (!buff) {
        DEBUG("new_hd2_buffer: kmalloc");
        return -ENOMEM;
    }

    if ((err = harddoom2_init_dma_buff(hd2, &buff->dma_buff, size))) {
        DEBUG("new_hd2_buffer: init_buffer");
        goto out_buff;
    }

    buff->hd2 = hd2;
    buff->width = width;
    buff->height = height;

    int flags = O_RDWR | O_CLOEXEC;

    int fd = get_unused_fd_flags(flags);
    if (fd < 0) {
        DEBUG("new_hd2_buffer: get unused fd, %d", fd);
        err = fd;
        goto out_getfd;
    }

    struct file* f = anon_inode_getfile("[harddoom2]", &hd2_buff_ops, buff, flags);
    if (IS_ERR(f)) {
        DEBUG("new_hd2_buffer: getfile");
        err = PTR_ERR(f);
        goto out_getfile;
    }

    f->f_mode |= FMODE_LSEEK | FMODE_PREAD | FMODE_PWRITE;
    buff->f = f;

    fd_install(fd, f);
    /* TODO: need to do anything else? */

    return fd;

out_getfile:
    put_unused_fd(fd);
out_getfd:
    free_dma_buff(&buff->dma_buff);
out_buff:
    kfree(buff);
    return err;
}

bool is_surface(const struct hd2_buffer* buff) {
    return buff->width != 0;
}

uint16_t get_buff_width(const struct hd2_buffer* buff) {
    /* We only perform this on surface buffers. */
    BUG_ON(!buff->width || !buff->height);
    return buff->width;
}

uint16_t get_buff_height(const struct hd2_buffer* buff) {
    /* We only perform this on surface buffers. */
    BUG_ON(!buff->width || !buff->height);
    return buff->height;
}

size_t get_buff_size(const struct hd2_buffer* buff) {
    return buff->dma_buff.size;
}

dma_addr_t get_page_table(struct hd2_buffer* buff) {
    return buff->dma_buff.page_table_dev;
}

void set_last_use(struct hd2_buffer* buff, struct counter cnt) {
    BUG_ON(!cnt_ge(cnt, buff->last_use));
    buff->last_use = cnt;
}

void set_last_write(struct hd2_buffer* buff, struct counter cnt) {
    BUG_ON(!cnt_ge(cnt, buff->last_write));
    buff->last_write = cnt;
}

struct hd2_buffer* hd2_buff_fd_get(int fd) {
    struct file* f = fget(fd);
    if (!f) {
        DEBUG("hd2_buff_fd_get: no file");
        return ERR_PTR(-EBADF);
    }

    if (f->f_op != &hd2_buff_ops) {
        DEBUG("hd2_buff_fd_get: wrong ops");
        fput(f);
        return ERR_PTR(-EINVAL);
    }

    struct hd2_buffer* buff = f->private_data;
    BUG_ON(buff->f != f);

    return buff;
}

void hd2_buff_get(struct hd2_buffer* buff) {
    get_file(buff->f);
}

void hd2_buff_put(struct hd2_buffer* buff) {
    if (buff) fput(buff->f);
}

void release_user_bufs(struct hd2_buffer* bufs[NUM_USER_BUFS]) {
    for (int i = 0; i < NUM_USER_BUFS; ++i) {
        hd2_buff_put(bufs[i]);
    }
}
