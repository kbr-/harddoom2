#ifndef HD2_H
#define HD2_H

#include "counter.h"
#include "buffer.h"

struct harddoom2;

int harddoom2_create_surface(struct harddoom2* hd2, struct doomdev2_ioctl_create_surface __user* _params);

int harddoom2_create_buffer(struct context* ctx, struct doomdev2_ioctl_create_buffer __user* _params);

/* Send as many commands in array 'cmds' with size 'num_cmds' as possible to the device using buffers 'bufs'.
   It is assumed that the given commands are valid with respect to the given buffers.
   Returns the number of commands written or negative error code. */
ssize_t harddoom2_write(struct harddoom2* hd2, struct fd* bufs, const struct doomdev2_cmd* cmds, size_t num_cmds);

#endif
