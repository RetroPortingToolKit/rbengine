#include "retcomm_rbengine/snap_ring.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>

typedef struct Object { unsigned *released; unsigned char page[4096]; } Object;
static void release(void *p) { Object *o = p; ++*o->released; free(o); }
static int read_object(const void *p, size_t offset, void *out, size_t count)
{ memcpy(out, ((const Object *)p)->page + offset, count); return 1; }
static RbeSnapObject make_object(unsigned *released, unsigned char value)
{
    Object *o = malloc(sizeof(*o));
    RbeSnapObject result;
    assert(o); o->released = released; memset(o->page, value, sizeof(o->page));
    result.object=o; result.logical_size=sizeof(o->page); result.release=release; result.read=read_object;
    return result;
}
#if defined(RBE_SNAP_CAPTURE_API_VERSION)
static int capture(void *ctx, uint32_t tick, uint8_t **p, size_t *capacity, size_t *size)
{
    (void)ctx; (void)tick;
    if (*capacity < 17) { *p = realloc(*p,17); assert(*p); *capacity=17; }
    memset(*p,7,17); *size=17; return 1;
}
#endif
int main(void)
{
    unsigned released=0; unsigned char bytes[7]; size_t size=99;
    RbeSnapRing *r=rbe_snap_ring_create(2);
    RbeSnapObject a=make_object(&released,4), b=make_object(&released,8), invalid=a;
    const RbeSnapObject *borrow;
    assert(r); assert(rbe_snap_ring_store_object(r,10,&a));
    borrow=rbe_snap_ring_peek_object(r,10);
    assert(borrow && borrow->object==a.object); /* same handle, no flatten */
    assert(!rbe_snap_ring_peek(r,10,&size) && size==0);
    assert(rbe_snap_ring_read_object(r,10,4090,bytes,6) && bytes[5]==4);
    assert(!rbe_snap_ring_read_object(r,10,4090,bytes,7));
    assert(!rbe_snap_ring_read_object(r,10,(size_t)-1,bytes,1));
    assert(!rbe_snap_ring_read_object(r,10,0,NULL,1));
    assert(rbe_snap_ring_read_object(r,10,4096,NULL,0));
    invalid.logical_size=0;
    assert(!rbe_snap_ring_store_object(r,10,&invalid));
    assert(rbe_snap_ring_peek_object(r,10)->object==a.object && released==0);
    assert(rbe_snap_ring_store_object(r,20,&b));
    assert(borrow->object==a.object && rbe_snap_ring_count(r)==2);
    a=make_object(&released,3); assert(rbe_snap_ring_store_object(r,10,&a));
    assert(released==1 && rbe_snap_ring_count(r)==2);
    a=make_object(&released,2); assert(rbe_snap_ring_store_object(r,0,&a));
    assert(released==2 && !rbe_snap_ring_has(r,10)); /* oldest numeric tick */
    assert(rbe_snap_ring_oldest_tick(r)==0);
    assert(rbe_snap_ring_drop_after(r,0)==1 && released==3);
    assert(rbe_snap_ring_drop_tick(r,0) && released==4);
    assert(!rbe_snap_ring_count(r));
    a=make_object(&released,5); assert(rbe_snap_ring_store_object(r,UINT32_MAX,&a));
    #if defined(RBE_SNAP_CAPTURE_API_VERSION)
    assert(rbe_snap_ring_capture(r,UINT32_MAX,capture,NULL)); /* object -> flat */
    #else
    { uint8_t *flat=malloc(17); assert(flat); memset(flat,7,17);
      assert(rbe_snap_ring_store(r,UINT32_MAX,flat,17)); }
    #endif
    assert(released==5 && rbe_snap_ring_count(r)==1 && !rbe_snap_ring_peek_object(r,UINT32_MAX));
    assert(rbe_snap_ring_peek(r,UINT32_MAX,&size) && size==17);
    a=make_object(&released,6); assert(rbe_snap_ring_store_object(r,UINT32_MAX,&a)); /* flat -> object */
    rbe_snap_ring_clear(r); assert(released==6 && !rbe_snap_ring_count(r));
    a=make_object(&released,9); assert(rbe_snap_ring_store_object(r,1,&a));
    rbe_snap_ring_destroy(r); assert(released==7);
    a=make_object(&released,1); assert(!rbe_snap_ring_store_object(NULL,0,&a));
    assert(released==7); a.release(a.object); assert(released==8);
    return 0;
}
