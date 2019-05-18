#ifndef FENCE_H
#define FENCE_H

#include "counter.h"

void wait_for_fence_cnt(struct harddoom2* hd2, struct counter cnt);

/* Do this at least every (2^32 - 1)th command. */
void update_last_fence_cnt(struct harddoom2* hd2);

#endif
