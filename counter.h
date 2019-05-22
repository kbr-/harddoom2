#ifndef COUNTER_H
#define COUNTER_H

#include <linux/types.h>

typedef uint64_t counter;

uint32_t cnt_lower(counter cnt);
uint32_t cnt_upper(counter cnt);
counter make_cnt(uint64_t upper, uint64_t lower);

#endif
