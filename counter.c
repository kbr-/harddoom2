#include "counter.h"

uint32_t cnt_lower(counter cnt) {
    return cnt;
}

uint32_t cnt_upper(counter cnt) {
    return cnt >> 32;
}

counter make_cnt(uint64_t upper, uint64_t lower) {
    return (upper << 32) + lower;
}
