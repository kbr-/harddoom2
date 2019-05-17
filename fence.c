#include "harddoom2.h"
#include "fence.h"

void handle_fence(struct harddoom2* hd2, uint32_t bit) {
    DEBUG("handle fence");

    wake_up(&hd2->fence_wq);
}

static void _update_last_fence_cnt(struct harddoom2* hd2) {
    uint32_t curr_lower = ioread32(hd2->bar + HARDDOOM2_FENCE_COUNTER);
    uint32_t last_lower = cnt_lower(hd2->last_fence_cnt);
    if (last_lower > curr_lower) {
        hd2->last_fence_cnt = make_cnt(cnt_upper(hd2->last_fence_cnt) + 1, curr_lower);
    }
}

void update_last_fence_cnt(struct harddoom2* hd2) {
    /* TODO spinlock */
    _update_last_fence_cnt(hd2);
}

void bump_fence_wait(struct harddoom2* hd2, struct counter cnt) {
    /* TODO spinlock */
    if (!cnt_ge(cnt, hd2->last_fence_wait)) {
        goto out;
    }

    iowrite(cnt_lower(cnt), hd2->bar + HARDDOOM2_FENCE_WAIT);
    hd2->last_fence_wait = cnt;

out:
    /* TODO spinunlock */
    return;
}

struct counter get_curr_fence_cnt(struct harddoom2* hd2) {
    struct counter res;
    /* TODO spinlock */
    _update_last_fence_cnt(hd2);
    res = hd2->last_fence_cnt;
    /* TODO spinunlock */
    return res;
}

void wait_for_fence_cnt(struct harddoom2* hd2, struct counter cnt) {
    if (cnt_ge(get_curr_fence_cnt(hd2), cnt)) {
        return;
    }

    bump_fence_wait(cnt);
    if (cnt_ge(get_curr_fence_cnt(hd2), cnt)) {
        return;
    }

    DEBUG("wait for fence: %lu", cnt.val);

    wait_event(&hd2->fence_wq, cnt_ge(get_curr_fence_cnt(hd2), cnt));

    DEBUG("wait for fence: %lu finished", cnt.val);
}
