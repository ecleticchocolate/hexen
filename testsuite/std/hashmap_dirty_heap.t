//@ use std/option.t std/iterator.t std/vector.t std/array.t std/hashmap.t
//@ expect stdout
//@ | len=2 m1=100 m2=200
//@ | vec=7
// `new` does not zero its allocation, so a container must not assume a fresh
// block is clean. This dirties the heap and frees it first, so the next malloc
// recycles non-zero bytes -- HashMap read those as slot.state==1 ("live") and
// silently returned the wrong value.
extern fn printf(u8* fmt, ...) i32
extern fn malloc(u64 n) u8*
extern fn free(u8* p)
fn main() i32 {
    u8* junk = malloc(65536)
    for u64 i = 0 to 65536 { junk[i] = 255 }
    free(junk)
    HashMap[i32, i32] m = HashMap[i32, i32].create()
    m[1] = 100
    m[2] = 200
    printf("len=%d m1=%d m2=%d\n", (i32)m.len(), m[1], m[2])
    Vector[i32] v = Vector[i32].create()
    v.push(7)
    printf("vec=%d\n", v[0])
    return 0
}
