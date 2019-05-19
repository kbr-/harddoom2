#ifndef DMA_BUFFER_H
#define DMA_BUFFER_H

#include "harddoom2.h"

#define MAX_BUFFER_PAGES 1024

/* 4K */
#define PAGE_SIZE HARDDOOM2_PAGE_SIZE

struct dma_buffer {
    void* pages_kern[MAX_BUFFER_PAGES];
    dma_addr_t pages_dev[MAX_BUFFER_PAGES];

    void* page_table_kern;
    dma_addr_t page_table_dev;

    size_t size;

    struct device* dev;
};

int init_dma_buff(struct dma_buffer* buff, size_t size, struct device* dev);
void free_dma_buff(struct dma_buffer* buff);

void write_dma_buff(struct dma_buffer* buff, void* src, size_t dst_pos, size_t size);

ssize_t write_dma_buff_user(struct dma_buffer* buff, void __user* src, size_t dst_pos, size_t size);
ssize_t read_dma_buff_user(struct dma_buffer* buff, void __user* dst, size_t src_pos, size_t size);

#endif
