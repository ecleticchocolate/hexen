//@ use std/option.t std/iterator.t std/vector.t std/array.t std/hashmap.t
//@ expect stdout
//@ | n=3 keysum=6 valsum=60
//@ | after_write=600
// Iteration yields ENTRIES, so keys are visible: `for unpack {k, v}`. Yielding
// V* made that impossible -- there was no pair to destructure. {k, *v} binds
// the value by pointer and writes through to the stored slot.
extern fn printf(u8* fmt, ...) i32
fn main() i32 {
    HashMap[i32, i32] m = { {1, 10}, {2, 20}, {3, 30} }
    i32 n = 0
    i32 ks = 0
    i32 vs = 0
    for unpack {k, v} in m { n = n + 1  ks = ks + k  vs = vs + v }
    printf("n=%d keysum=%d valsum=%d\n", n, ks, vs)
    for unpack {k, *v} in m { *v = *v * 10 }
    i32 tot = 0
    for unpack {k, v} in m { tot = tot + v }
    printf("after_write=%d\n", tot)
    return 0
}
