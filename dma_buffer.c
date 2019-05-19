#include "dma_buffer.h"

_Static_assert(SSIZE_MAX >= MAX_BUFFER_PAGES * PAGE_SIZE, "ssize_max too small");

int init_dma_buff(struct dma_buffer* buff, size_t size, struct device* dev) {
    BUG_ON(size > MAX_BUFFER_PAGES * PAGE_SIZE);

    buff->page_table_kern = dma_alloc_coherent(dev, PAGE_SIZE, &buff->page_table_dev, GFP_KERNEL);
    if (!buff->page_table_kern) {
        DEBUG("init_dma_buff: page_table_kern");
        return -ENOMEM;
    }
    if (buff->page_table_dev & 0xff) {
        DEBUG("init_dma_buff: alignment 256");
        goto out_table;
    }

    uint32_t* page_table = buff->page_table_kern;
    size_t num_pages = (size + PAGE_SIZE - 1) / PAGE_SIZE;
    BUG_ON(page_table + num_pages - (buff->page_table_kern + PAGE_SIZE) > 0);

    size_t page;
    for (page = 0; page < num_pages; ++page) {
        buff->pages_kern[page] = dma_alloc_coherent(dev, PAGE_SIZE, &buff->pages_dev[page], GFP_KERNEL);
        if (!buff->pages_kern[page]) {
            DEBUG("init_buffer: page_kern %lu", page);
            goto out_pages;
        }
        if (buff->pages_dev[page] & 0xfff) {
            DEBUG("init_dma_buf: alignment 4K");
            ++page;
            goto out_pages;
        }

        /* bits 0 and 1 are on, 2 and 3 are off, bits 4-31 are equal to bits 12-39 of the page's address */
        page_table[page] = ((buff->pages_dev[page] >> 12) << 4) | 3;
    }

    buff->size = size;
    buff->dev = dev;

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

void free_dma_buff(struct dma_buffer* buff) {
    struct device* dev = &buff->dev;
    size_t num_pages = (buff->size + PAGE_SIZE - 1) / PAGE_SIZE;

    size_t page;
    for (page = 0; page < num_pages; ++page) {
        dma_free_coherent(dev, PAGE_SIZE, buff->pages_kern[page], buff->pages_dev[page]);
    }

    dma_free_coherent(dev, PAGE_SIZE, buff->page_table_kern, buff->page_table_dev);
}

void write_dma_buff(struct buffer* buff, const void* src, size_t dst_pos, size_t size) {
    BUG_ON(dst_pos + size < dst_pos || dst_pos + size > buff->size);

    size_t page = dst_pos / PAGE_SIZE;
    size_t page_off = dst_pos % PAGE_SIZE;

    void* dst = pages_kern[page] + page_off;

    size_t space_in_page = PAGE_SIZE - page_off;
    if (space_in_page > size) {
        space_in_page = size;
    }

    memcpy(dst, src, space_in_page);
    src += space_in_page;
    size -= space_in_page;

    while (size >= PAGE_SIZE) {
        BUG_ON(page >= (buff->size + PAGE_SIZE - 1) / PAGE_SIZE);

        dst = pages_kern[++page];
        memcpy(dst, src, PAGE_SIZE);
        src += PAGE_SIZE;
        size -= PAGE_SIZE;
    }

    if (size) {
        BUG_ON(page >= (buff->size + PAGE_SIZE - 1) / PAGE_SIZE);
        dst = pages_kern[++page];
        memcpy(dst, src, size);
    }
}

ssize_t write_dma_buff_user(struct buffer* buff, const void __user* src, size_t dst_pos, size_t size) {
    BUG_ON(dst_pos + size < dst_pos || dst_pos + size > buff->size);

    size_t page = dst_pos / PAGE_SIZE;
    size_t page_off = dst_pos % PAGE_SIZE;

    void* dst = pages_kern[page] + page_off;

    size_t space_in_page = PAGE_SIZE - page_off;
    if (space_in_page > size) {
        space_in_page = size;
    }

    unsigned long left = copy_from_user(dst, src, space_in_page);
    ssize_t copied = space_in_page - left;
    if (left) {
        DEBUG("write_dma_buff_user: copy_from_user fail");
        goto out_copy;
    }

    src += space_in_page;
    size -= space_in_page;

    while (size >= PAGE_SIZE) {
        BUG_ON(page >= (buff->size + PAGE_SIZE - 1) / PAGE_SIZE);

        dst = pages_kern[++page];
        left = copy_from_user(dst, src, PAGE_SIZE);
        copied += PAGE_SIZE - left;
        if (left) {
            DEBUG("write_dma_buff_user: copy_from_user fail");
            goto out_copy;
        }

        src += PAGE_SIZE;
        size -= PAGE_SIZE;
    }

    if (size) {
        BUG_ON(page >= (buff->size + PAGE_SIZE - 1) / PAGE_SIZE);
        dst = pages_kern[++page];
        left = copy_from_user(dst, src, size);
        copied += size - left;
        if (left) {
            DEBUG("write_dma_buff_user: copy_from_user fail");
            goto out_copy;
        }
    }

    return copied;

out_copy:
    if (copied)
        return copied;
    return -EFAULT;
}

ssize_t read_dma_buff_user(const struct dma_buffer* buff, void __user* dst, size_t src_pos, size_t size) {
    BUG_ON(src_pos + size < src_pos || src_pos + size > buff->size);

    size_t page = src_pos / PAGE_SIZE;
    size_t page_off = src_pos % PAGE_SIZE;

    void* src = pages_kern[page] + page_off;

    size_t space_in_page = PAGE_SIZE - page_off;
    if (space_in_page > size) {
        space_in_page = size;
    }

    unsigned long left = copy_to_user(dst, src, space_in_page);
    ssize_t copied = space_in_page - left;
    if (left) {
        DEBUG("read_dma_buff_user: copy to user fail");
        goto out_copy;
    }

    dst += space_in_page;
    size -= space_in_page;

    while (size >= PAGE_SIZE) {
        BUG_ON(page >= (buff->size + PAGE_SIZE - 1) / PAGE_SIZE);

        src = pages_kern[++page];
        left = copy_to_user(dst, src, PAGE_SIZE);
        copied += PAGE_SIZE - left;
        if (left) {
            DEBUG("read_dma_buff_user: copy to user fail");
            goto out_copy;
        }

        dst += PAGE_SIZE;
        size -= PAGE_SIZE;
    }

    if (size) {
        BUG_ON(page >= (buff->size + PAGE_SIZE - 1) / PAGE_SIZE);

        src = pages_kern[++page];
        left = copy_to_user(dst, src, size);
        copied += size - left;
        if (left) {
            DEBUG("read_dma_buff_user: copy to user fail");
            goto out_copy;
        }
    }

    return copied;

out_copy:
    if (copied)
        return copied;
    return -EFAULT;
}
