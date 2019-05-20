#ifndef HD2_BUFFER_H
#define HD2_BUFFER_H

#include "counter.h"

#define NUM_USER_BUFS 7

#define DST_BUF_IDX 0
#define TEXTURE_BUF_IDX 2
#define COLORMAP_BUF_IDX 4
#define TRANSLATE_BUF_IDX  5
#define TRANMAP_BUF_IDX 6

struct harddoom2;
struct hd2_buffer;

/* Open a new file representing a buffer and return its file descriptor. */
int new_hd2_buffer(struct harddoom2* hd2, size_t size, uint16_t width, uint16_t height);

bool is_surface(struct hd2_buffer*);
uint16_t get_buff_width(struct hd2_buffer*);
uint16_t get_buff_height(struct hd2_buffer*);
size_t get_buff_size(struct hd2_buffer*);
dma_addr_t get_page_table(struct hd2_buffer*);

void set_last_use(struct hd2_buffer*, struct counter cnt);
void set_last_write(struct hd2_buffer*, struct counter cnt);

struct hd2_buffer* hd2_buff_fd_get(int fd);
void hd2_buff_get(struct hd2_buffer*);
void hd2_buff_put(struct hd2_buffer*);

void release_user_bufs(struct hd2_buffer* bufs[NUM_USER_BUFS]);

#endif
