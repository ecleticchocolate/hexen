//@ use std/option.t std/iterator.t std/vector.t std/array.t std/hashmap.t
//@ expect stdout
//@ | len=2
//@ | m1=10 m2=20
// A map initialized straight from a literal of pairs. The literal is checked
// against Pair[K,V], NOT Entry -- Entry's third `state` byte is internal and a
// caller must never have to write it.
extern fn printf(u8* fmt, ...) i32
fn main() i32 {
    HashMap[i32, i32] m = { {1, 10}, {2, 20} }
    printf("len=%d\n", (i32)m.len())
    printf("m1=%d m2=%d\n", m[1], m[2])
    return 0
}
