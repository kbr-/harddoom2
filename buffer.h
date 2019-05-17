#include "hd2.h"

struct buffer {
    void* pages_kern[1024];
    dma_addr_t pages_dev[1024];

    void* page_table_kern;
    dma_addr_t page_table_dev;

    struct harddoom2* hd2;

    size_t size;

    struct counter last_use;
    struct counter last_write;

    /* Non-zero values indicate that this is a surface buffer.
       Zero indicates any other type of buffer (cmd, texture, etc.).
       Used to check correctness of received setups */
    uint16_t width;
    uint16_t height;
};
