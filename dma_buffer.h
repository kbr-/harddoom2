#ifndef DMA_BUFFER_H
#define DMA_BUFFER_H

#include <linux/types.h>

#define MAX_BUFFER_PAGES 1024

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

void write_dma_buff(struct dma_buffer* buff, const void* src, size_t dst_pos, size_t size);

ssize_t write_dma_buff_user(struct dma_buffer* buff, const void __user* src, size_t dst_pos, size_t size);
ssize_t read_dma_buff_user(const struct dma_buffer* buff, void __user* dst, size_t src_pos, size_t size);

#endif
