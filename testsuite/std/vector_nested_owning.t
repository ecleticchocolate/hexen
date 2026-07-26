//@ use std/option.t std/iterator.t std/vector.t std/array.t std/hashmap.t
//@ expect stdout
//@ | outer=3
//@ | inner=1 v0=11
//@ | deep_copy=1
extern fn printf(u8* fmt, ...) i32
fn mk(u8 v) Vector[u8] { Vector[u8] i = Vector[u8].create()  i.push(v)  return i }
fn main() i32 {
    Vector[Vector[u8]] o = Vector[Vector[u8]].create()
    for u32 i = 0 to 3 { o.push(mk((u8)(10 + i))) }
    printf("outer=%d\n", (i32)o.len())
    match o.get(1) { {.Some = *p} { printf("inner=%d v0=%d\n", (i32)p.len(), (i32)(*p)[0]) }  .None {} }
    Vector[Vector[u8]] c = o.copy()
    printf("deep_copy=%d\n", (i32)(o == c))
    return 0
}
