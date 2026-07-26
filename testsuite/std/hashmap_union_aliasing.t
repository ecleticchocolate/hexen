//@ use std/option.t std/iterator.t std/vector.t std/array.t std/hashmap.t
//@ expect stdout
//@ | via_i=7
//@ | via_u=7
//@ | len=1
// A union's members OVERLAP, so writing .i then reading .u is the same bytes.
// Two keys written through different members with the same bit pattern are the
// same key -- there is no tag to say otherwise, and the structural walk hashes
// the shared storage rather than a "current member" it cannot know.
extern fn printf(u8* fmt, ...) i32
union U { i32 i  u32 u }
fn main() i32 {
    HashMap[U, i32] m = HashMap[U, i32].create()
    U a
    a.i = 7
    m[a] = 7
    printf("via_i=%d\n", m[a])
    U b
    b.u = 7
    printf("via_u=%d\n", m[b])
    printf("len=%d\n", (i32)m.len())
    return 0
}
