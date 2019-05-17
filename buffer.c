#include "buffer.h"

int init_buffer(struct buffer* buff, struct harddoom2* hd2, size_t size) {
    if (size > MAX_BUFFER_SIZE) { ERROR("bad init_buffer"); return -EINVAL; }

    struct device* dev = &hd2->pdev->dev;

    buff->page_table_kern = dma_alloc_coherent(dev, PAGE_SIZE, &buff->page_table_dev, GFP_KERNEL);
    if (!buff->page_table_kern) {
        DEBUG("init_buffer: page_table_kern");
        goto out_table;
    }

    uint32_t* page_table = (uint32_t*)buff->page_table_kern;

    size_t num_pages = (size + PAGE_SIZE - 1) / PAGE_SIZE;
    size_t page;
    for (page = 0; page < num_pages; ++page) {
        void* buff->pages_kern[page] = dma_alloc_coherent(dev, PAGE_SIZE, &buff->pages_dev[page], GFP_KERNEL);
        if (!page_kern) {
            DEBUG("init_buffer: page_kern");
            goto out_pages;
        }

        page_table[page] = (uint32_t)(buff->pages_dev[page] >> 4) | 3; /* TODO: think about this */
    }

    buff->hd2 = hd2;
    buff->size = size;
    buff->last_use.val = 0;
    buff->last_write.val = 0;
    buff->width = 0;
    buff->height = 0;

    return 0;

out_pages:
    num_pages = page;
    for (page = 0; page < num_pages; ++page) {
        dma_free_coherent(dev, PAGE_SIZE, buff->pages_kern[page], buff->pages_dev[page]);
    }

out_table:
    dma_free_coherent(dev, PAGE_SIZE, buff->page_table_kern, buff->page_table_dev);

    return -ENOMEM;
}

void free_buffer(struct buffer* buff) {
    struct device* dev = &buff->hd2->pdev->dev;
    size_t num_pages = (buff->size + PAGE_SIZE - 1) / PAGE_SIZE;

    size_t page;
    for (page = 0; page < num_pages; ++page) {
        dma_free_coherent(dev, PAGE_SIZE, buff->pages_kern[page], buff->pages_dev[page]);
    }

    dma_free_coherent(dev, PAGE_SIZE, buff->page_table_kern, buff->page_table_dev);
}

int write_buffer(struct buffer* buff, void* src, size_t dst_pos, size_t size) {
    if (dst_pos >= buff->size || size > buff->size || dst_pos + size > buff->size) { ERROR("bad write_buffer!"); return -EINVAL; }

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

int write_buffer_user(struct buffer* buff, void __user* src, size_t dst_pos, size_t size, size_t* bytes_transferred) {
    if (dst_pos >= buff->size || size > buff->size || dst_pos + size > buff->size) { ERROR("bad write_buffer!"); return -EINVAL; }

    size_t page = src_pos / PAGE_SIZE;
    size_t page_off = src_pos % PAGE_SIZE;
    size_t space_in_page = PAGE_SIZE - page_off;
    void* src = pages_kern[page] + page_off;
    *bytes_transferred = 0;

    while (size) {
        if (space_in_page > size) {
            space_in_page = size;
        }

        unsigned long left = copy_from_user(dst, src, space_in_page);
        *bytes_transferred += (space_in_page - left);
        if (left) {
            DEBUG("write_buffer_user: copy_from_user fail");
            return -EFAULT;
        }

        src += space_in_page;
        size -= space_in_page;

        dst = pages_kern[++page];
        space_in_page = PAGE_SIZE;
    }

    return 0;
}


int read_buffer_user(struct buffer* buff, void __user* dst, size_t src_pos, size_t size, size_t* bytes_transferred) {
    if (src_pos >= buff->size || size > buff->size || src_pos + size > buff->size) { ERROR("bad read_buffer!"); return -EINVAL; }

    size_t page = src_pos / PAGE_SIZE;
    size_t page_off = src_pos % PAGE_SIZE;
    size_t space_in_page = PAGE_SIZE - page_off;
    void* src = pages_kern[page] + page_off;
    *bytes_transferred = 0;

    while (size) {
        if (space_in_page > size) {
            space_in_page = size;
        }

        unsigned long left = copy_to_user(dst, src, space_in_page);
        *bytes_transferred += (space_in_page - left);
        if (left) {
            DEBUG("read_buffer_user: copy_to_user fail");
            return -EFAULT;
        }

        dst += space_in_page;
        size -= space_in_page;

        src = pages_kern[++page];
        space_in_page = PAGE_SIZE;
    }

    return 0;
}
