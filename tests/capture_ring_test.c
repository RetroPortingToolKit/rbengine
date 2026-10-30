#include "retcomm_rbengine/snap_ring.h"
#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

typedef struct Context {
    RbeSnapRing *ring;
    size_t wanted;
    unsigned grows;
    int fail, invalid, require_empty;
} Context;

static int capture(void *opaque, uint32_t tick, uint8_t **data,
                    size_t *capacity, size_t *size) {
    Context *c = (Context *)opaque;
    uint32_t t;
    if (c->require_empty) assert(!*data && !*capacity);
    for (t = 0; t < 32; ++t) {
        const uint8_t *visible = rbe_snap_ring_peek(c->ring, t, NULL);
        assert(!visible || visible != *data);
    }
    if (*capacity < c->wanted) {
        uint8_t *fresh = (uint8_t *)realloc(*data, c->wanted);
        assert(fresh);
        *data = fresh;
        *capacity = c->wanted;
        ++c->grows;
    }
    memset(*data, (int)tick, c->wanted);
    *size = c->invalid ? *capacity + 1 : c->wanted;
    return !c->fail;
}

static void check_blob(RbeSnapRing *r, uint32_t tick, size_t expected) {
    size_t n, i;
    const uint8_t *p = rbe_snap_ring_peek(r, tick, &n);
    assert(p && n == expected);
    for (i = 0; i < n; ++i) assert(p[i] == (uint8_t)tick);
}

int main(void) {
    RbeSnapRing *r = rbe_snap_ring_create(3);
    Context c = {r, 128, 0, 0, 0, 0};
    uint32_t t;
    assert(r);
    for (t = 0; t < 16; ++t) {
        assert(rbe_snap_ring_capture(r, t, capture, &c));
        check_blob(r, t, c.wanted);
        assert(rbe_snap_ring_count(r) == (t < 3 ? t + 1 : 3));
    }
    assert(c.grows == 4); /* depth plus one spare, then no allocator work */
    c.fail = 1;
    assert(!rbe_snap_ring_capture(r, 15, capture, &c));
    check_blob(r, 13, 128); check_blob(r, 14, 128); check_blob(r, 15, 128);
    assert(rbe_snap_ring_count(r) == 3 && !rbe_snap_ring_has(r, 16));
    c.fail = 0; c.invalid = 1;
    assert(!rbe_snap_ring_capture(r, 16, capture, &c));
    check_blob(r, 13, 128); check_blob(r, 15, 128);
    c.invalid = 0; c.wanted = 512;
    for (t = 16; t < 24; ++t) assert(rbe_snap_ring_capture(r, t, capture, &c));
    assert(c.grows == 8);
    check_blob(r, 21, 512); check_blob(r, 23, 512);
    c.wanted = 17;
    assert(rbe_snap_ring_capture(r, 22, capture, &c));
    check_blob(r, 22, 17); /* new length, capacity stays reusable */
    assert(c.grows == 8 && rbe_snap_ring_count(r) == 3);
    assert(rbe_snap_ring_drop_tick(r, 21));
    assert(rbe_snap_ring_drop_after(r, 22) == 1);
    check_blob(r, 22, 17);
    assert(rbe_snap_ring_count(r) == 1);
    rbe_snap_ring_clear(r);
    c.require_empty = 1;
    assert(rbe_snap_ring_capture(r, 1, capture, &c));
    c.require_empty = 0;
    {
        uint8_t *p = (uint8_t *)malloc(7);
        assert(p); memset(p, 2, 7);
        assert(rbe_snap_ring_store(r, 2, p, 7));
    }
    assert(rbe_snap_ring_capture(r, 2, capture, &c));
    check_blob(r, 1, 17); check_blob(r, 2, 17);
    rbe_snap_ring_destroy(r); /* also owns the spare from mixed store/capture */
    return 0;
}
