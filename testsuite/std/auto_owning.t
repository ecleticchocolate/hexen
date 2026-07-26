//@ use std/option.t std/iterator.t std/vector.t std/array.t std/hashmap.t
//@ expect stdout
//@ | inner=1
//@ | survived
// `auto` on an OWNING value: the binding is a real local of the inferred type,
// so scope-exit RAII destroys it exactly once -- it is not a second alias.
extern fn printf(u8* fmt, ...) i32
fn mk(u8 v) Vector[u8] { Vector[u8] i = Vector[u8].create()  i.push(v)  return i }
fn main() i32 {
    auto outer = Vector[Vector[u8]].create()
    outer.push(mk(3))
    match outer.get(0) { {.Some = *p} { printf("inner=%d\n", (i32)p.len()) }  .None {} }
    printf("survived\n")
    return 0
}
