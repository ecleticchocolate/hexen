//@ use std/option.t std/iterator.t std/vector.t std/array.t std/hashmap.t
//@ expect stdout
//@ | vec len=0 cap=0
//@ | map len=0 cap=0
//@ | vec works=1
//@ | map works=42
// Storage is NOT zeroed. A bare declaration writes only the FIELD DEFAULTS the
// type declares, so a container's "empty" state is something the type states
// explicitly rather than something the allocator happens to provide. The heap
// is dirtied first so a recycled block would be visibly non-zero.
extern fn printf(u8* fmt, ...) i32
extern fn malloc(u64 n) u8*
extern fn free(u8* p)
fn main() i32 {
    u8* junk = malloc(65536)
    for u64 i = 0 to 65536 { junk[i] = 255 }
    free(junk)
    Vector[i32] v
    printf("vec len=%d cap=%d\n", (i32)v.len(), (i32)v.cap())
    HashMap[i32, i32] m
    printf("map len=%d cap=%d\n", (i32)m.len(), (i32)m.cap())
    v.push(1)
    printf("vec works=%d\n", v[0])
    m[7] = 42
    printf("map works=%d\n", m[7])
    return 0
}
