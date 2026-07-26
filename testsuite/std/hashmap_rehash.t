//@ use std/option.t std/iterator.t std/vector.t std/array.t std/hashmap.t
//@ expect stdout
//@ | len=40
//@ | first=0 last=390
// Growth must rehash every live entry into the new table, losing none.
extern fn printf(u8* fmt, ...) i32
fn main() i32 {
    HashMap[i32, i32] m = HashMap[i32, i32].with_capacity(4)
    for u32 i = 0 to 40 { m[(i32)i] = (i32)(i * 10) }
    printf("len=%d\n", (i32)m.len())
    printf("first=%d last=%d\n", m[0], m[39])
    return 0
}
