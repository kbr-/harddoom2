#include "dma_buffer.h"
#include "hd2_buffer.h"

struct hd2_buffer {
    struct dma_buffer;

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

uint16_t get_buff_width(struct hd2_buffer* buff) {
    /* We only perform this on surface buffers. */
    BUG_ON(!buff->width || !buff->height);
    return buff->width;
}

uint16_t get_buff_height(struct hd2_buffer* buff) {
    /* We only perform this on surface buffers. */
    BUG_ON(!buff->width || !buff->height);
    return buff->height;
}

dma_addr_t get_page_table(struct hd2_buffer* buff) {
    return buff->page_table_dev;
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
        return NULL;
    }

    if (f->f_op != hd2_buff_ops) {
        DEBUG("hd2_buff_fd_get: wrong ops");
        fput(f);
        return NULL;
    }

    struct hd2_buffer* buff = f->private_data;
    BUG_ON(buff->f != f);

    return buff;
}

struct hd2_buffer* hd2_buff_get(struct hd2_buffer* buff) {
    get_file(buff->f);
    return buff;
}

void hd2_buff_put(struct hd2_buffer* buff) {
    if (buff) fput(buff->f);
}

void release_user_bufs(struct hd2_buffer* bufs[NUM_USER_BUFS]) {
    for (int i = 0; i < NUM_USER_BUFS; ++i) {
        hd2_buff_put(bufs[i]);
    }
}

int new_hd2_buffer(struct harddoom2* hd2, size_t size, uint16_t width, uint16_t height) {
    BUG_ON((width && size != width * height) || size > MAX_BUFFER_PAGES * HARDDOOM2_PAGE_SIZE);
    int err;

    struct hd2_buffer* buff = kzalloc(sizeof(struct hd2_buffer), GFP_KERNEL);
    if (!buff) {
        DEBUG("new_hd2_buffer: kmalloc");
        return -ENOMEM;
    }

    if ((err = init_dma_buff(buff, &hd2->pdev->dev, size))) {
        DEBUG("make_buffer: init_buffer");
        goto out_buff;
    }

    buff->hd2 = hd2;
    buff->width = width;
    buff->height = height;

    int flags = O_RDWR | O_CREAT;

    int fd = get_unused_fd_flags(flags);
    if (fd < 0) {
        DEBUG("make_buffer: get unused fd, %d", fd);
        err = fd;
        goto out_getfd;
    }

    struct file* f = anon_inode_getfile(DRV_NAME, hd2_buff_ops, buff, flags);
    if (IS_ERR(f)) {
        DEBUG("make_buffer: getfile");
        err = PTR_ERR(f);
        goto out_getfile;
    }

    f->f_mode = FMODE_LSEEK | FMODE_PREAD | FMORE_PWRITE;
    buff->f = f;

    fd_install(fd, f);
    /* TODO: need to do anything else? */

    return fd;

out_getfile:
    put_unused_fd(fd);
out_getfd:
    free_dma_buff(buff);
out_buff:
    kfree(buff);
    return err;
}
