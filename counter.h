#ifndef COUNTER_H
#define COUNTER_H

uint32_t cnt_lower(struct counter cnt);
uint32_t cnt_upper(struct counter cnt);
struct counter make_cnt(uint64_t upper, uint64_t lower);

#endif
