//@ use std/option.t std/iterator.t std/vector.t std/array.t std/hashmap.t
//@ expect val 4950
extern fn printf(u8* fmt, ...) i32
fn main() i32 {
    Vector[i32] v = Vector[i32].with_capacity(2)
    for u32 i = 0 to 100 { v.push((i32)i) }
    i32 sum = 0
    for i32* p in v { sum = sum + *p }
    return sum
}
