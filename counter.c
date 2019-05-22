#include "counter.h"

uint32_t cnt_lower(struct counter cnt) {
    return cnt.val;
}

uint32_t cnt_upper(struct counter cnt) {
    return cnt.val >> 32;
}

counter make_cnt(uint64_t upper, uint64_t lower) {
    return (upper << 32) + lower;
}
