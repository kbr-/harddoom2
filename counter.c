#include "counter.h"

void cnt_incr(struct counter* cnt) {
    ++cnt->val;
}

uint32_t cnt_lower(struct counter cnt) {
    return cnt.val;
}

uint32_t cnt_upper(struct counter cnt) {
    return cnt.val >> 32;
}

struct counter make_cnt(uint64_t upper, uint64_t lower) {
    return (struct counter){ .val = (upper << 32) + lower };
}

bool cnt_ge(struct counter cnt1, struct counter cnt2) {
    return cnt1.val >= cnt2.val;
}
