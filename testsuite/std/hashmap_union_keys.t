//@ use std/option.t std/iterator.t std/vector.t std/array.t std/hashmap.t
//@ expect stdout
//@ | same=1
//@ | len=1
// A union key: fields overlap with no tag, so there is no "active member" to
// walk. It still works as a key because the structural arms treat it like any
// other field list -- but note the SEMANTICS: two unions written through
// different members can only be equal if their bytes are, which is what a
// tagless overlap can mean.
extern fn printf(u8* fmt, ...) i32
union U { i32 i  u32 u }
fn main() i32 {
    HashMap[U, i32] m = HashMap[U, i32].create()
    U a
    a.i = 5
    U b
    b.i = 5
    m[a] = 1
    printf("same=%d\n", m[b])
    printf("len=%d\n", (i32)m.len())
    return 0
}
