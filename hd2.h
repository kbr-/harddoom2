#ifndef HD2_H
#define HD2_H

#include "doomdev2.h"

#include "counter.h"
#include "hd2_buffer.h"

struct harddoom2;
struct hd2_buffer;

struct harddoom2* get_hd2(unsigned num);

int harddoom2_create_surface(struct harddoom2* hd2, struct doomdev2_ioctl_create_surface __user* _params);

int harddoom2_create_buffer(struct harddoom2* hd2, struct doomdev2_ioctl_create_buffer __user* _params);

/* Send as many commands in array 'cmds' with size 'num_cmds' as possible to the device using buffers 'bufs'.
   It is assumed that the given commands are valid with respect to the given buffers.
   Returns the number of commands written or negative error code. */
ssize_t harddoom2_write(struct harddoom2* hd2, struct hd2_buffer* bufs[NUM_USER_BUFS],
        const struct doomdev2_cmd* cmds, size_t num_cmds);

void wait_for_fence_cnt(struct harddoom2* hd2, struct counter cnt);

#endif
