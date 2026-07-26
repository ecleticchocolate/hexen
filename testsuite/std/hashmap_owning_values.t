//@ use std/option.t std/iterator.t std/vector.t std/array.t std/hashmap.t
//@ expect stdout
//@ | inner=1 v0=4
//@ | copy_len=1
//@ | survived
// An owning V is deep-copied in and destroyed exactly once on teardown.
extern fn printf(u8* fmt, ...) i32
fn mk(u8 v) Vector[u8] { Vector[u8] i = Vector[u8].create()  i.push(v)  return i }
fn main() i32 {
    HashMap[i32, Vector[u8]] m = HashMap[i32, Vector[u8]].create()
    m.insert(1, mk(4))
    match m.get_ptr(1) { {.Some = p} { printf("inner=%d v0=%d\n", (i32)p.len(), (i32)(*p)[0]) }  .None {} }
    HashMap[i32, Vector[u8]] c = m.copy()
    printf("copy_len=%d\n", (i32)c.len())
    printf("survived\n")
    return 0
}
