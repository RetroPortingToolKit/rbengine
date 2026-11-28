#ifndef RETCOMM_RBENGINE_SNAP_RING_H
#define RETCOMM_RBENGINE_SNAP_RING_H

/*
 * Tick-addressable in-memory snapshot ring for rollback.
 *
 * Owns opaque blobs only. Engines serialize via RbeSnapVTable (or call
 * rbe_snap_ring_store with a pre-built blob).
 *
 * Default depth is 40, not a deep tick history:
 *   - MotK TipHold runway is 24 ticks (P cap 16; coalesced press→release
 *     can grow an episode toward 24).
 *   - Live interval snaps are sparse (16); FMV media snaps every 2–4.
 *     40 = 24 runway + ~16 denser-cadence / follower-lag slots, half the
 *     old ungrounded 80. Hosts that need more pass an explicit depth.
 *
 * DEPTH IS SLOTS, NOT TICKS, and the two differ by the host's snapshot
 * interval. The derivation above assumes the sparse interval of 16 that MotK
 * uses, so 40 slots there reach back roughly 640 ticks. A host that snapshots
 * every tick -- snesrecomp does, its replay being inline and cheap -- gets 40
 * TICKS from the same 40 slots, about a sixteenth of the reach, and the
 * arithmetic above does not transfer. It happens to remain sufficient there,
 * but for a different reason than this comment gives, so measure rather than
 * inherit: the reach a host needs is peer lag + prediction depth + runway, and
 * the failure below that is the follower NACKing every episode whose load tick
 * has aged out.
 */

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define RBE_SNAP_RING_DEFAULT_DEPTH 40u

typedef struct RbeSnapRing RbeSnapRing;

#define RBE_SNAP_OBJECT_API_VERSION 1
/* Optional immutable snapshot representation, e.g. shared pages. The ring does
 * not flatten or inspect it. logical_size bounds reads, not physical shared-page
 * storage. The host retains its page/resource accounting and serializer.
 * Callbacks must not reenter/mutate the ring and release must not throw.
 */
typedef struct RbeSnapObject {
    void *object;
    size_t logical_size;
    void (*release)(void *object);
    int (*read)(const void *object, size_t offset, void *destination, size_t count);
} RbeSnapObject;

/* Takes object ownership only on success. Construct the entire immutable object
 * before calling; invalid input/failure leaves prior history and ownership
 * unchanged. Successful replacement, eviction, drop, clear or destroy releases
 * exactly once. Do not submit an object already owned by the ring.
 * Flat and object entries share the same depth/tick/eviction policy. Flat peek
 * returns NULL for object entries, object peek returns NULL for flat entries.
 */
int rbe_snap_ring_store_object(RbeSnapRing *r, uint32_t tick, const RbeSnapObject *object);
/* Borrowed until this tick is replaced/evicted/dropped, or clear/destroy.
 * Reads never allocate/flatten and cannot outlive that borrowed lifetime.
 */
const RbeSnapObject *rbe_snap_ring_peek_object(const RbeSnapRing *r, uint32_t tick);
int rbe_snap_ring_read_object(const RbeSnapRing *r, uint32_t tick,
                             size_t offset, void *destination, size_t count);

/* Optional host serializer. serialize must malloc *out_data (ring takes
 * ownership on success). deserialize restores from a peek'd blob. */
typedef struct RbeSnapVTable {
    void *ctx;
    int (*serialize)(void *ctx, uint32_t tick, uint8_t **out_data, size_t *out_len);
    int (*deserialize)(void *ctx, uint32_t tick, const uint8_t *data, size_t len);
} RbeSnapVTable;

RbeSnapRing *rbe_snap_ring_create(uint32_t depth);
void         rbe_snap_ring_destroy(RbeSnapRing *r);
void         rbe_snap_ring_clear(RbeSnapRing *r);

uint32_t rbe_snap_ring_depth(const RbeSnapRing *r);
uint32_t rbe_snap_ring_count(const RbeSnapRing *r);
int      rbe_snap_ring_has(const RbeSnapRing *r, uint32_t tick);

/* Take ownership of data on success (caller must not free). Overwrites an
 * existing entry for the same tick. */
int rbe_snap_ring_store(RbeSnapRing *r, uint32_t tick, uint8_t *data, size_t size);

const uint8_t *rbe_snap_ring_peek(const RbeSnapRing *r, uint32_t tick, size_t *size_out);

/* Serialize via vtable then store. */
int rbe_snap_ring_save(RbeSnapRing *r, uint32_t tick, const RbeSnapVTable *vt);

/* Peek + deserialize via vtable. */
int rbe_snap_ring_load(RbeSnapRing *r, uint32_t tick, const RbeSnapVTable *vt);

uint32_t rbe_snap_ring_drop_after(RbeSnapRing *r, uint32_t tick);
int      rbe_snap_ring_drop_tick(RbeSnapRing *r, uint32_t tick);
uint32_t rbe_snap_ring_oldest_tick(const RbeSnapRing *r);
uint32_t rbe_snap_ring_newest_tick(const RbeSnapRing *r);

#ifdef __cplusplus
}
#endif

#endif /* RETCOMM_RBENGINE_SNAP_RING_H */
