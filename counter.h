#ifndef COUNTER_H
#define COUNTER_H

struct counter {
    uint64_t val;
}

void cnt_incr(struct counter* cnt);
uint32_t cnt_lower(struct counter cnt);
uint32_t cnt_upper(struct counter cnt);
struct counter make_cnt(uint64_t upper, uint64_t lower);
bool cnt_ge(struct counter cnt1, struct counter cnt2);

#endif
