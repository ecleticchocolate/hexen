//@ use std/option.t std/iterator.t std/vector.t std/array.t std/hashmap.t
//@ expect stdout
//@ | len=3 cap=4
//@ | idx=20
//@ | get=20
//@ | pop=30
//@ | eq=1 neq=0
//@ | after_clear=0
extern fn printf(u8* fmt, ...) i32
fn main() i32 {
    Vector[i32] v = Vector[i32].create()
    v.push(10)  v.push(20)  v.push(30)
    printf("len=%d cap=%d\n", (i32)v.len(), (i32)v.cap())
    printf("idx=%d\n", v[1])
    match v.get(1) { {.Some = x} { printf("get=%d\n", x) }  .None {} }
    match v.pop() { {.Some = x} { printf("pop=%d\n", x) }  .None {} }
    Vector[i32] c = v.copy()
    printf("eq=%d neq=%d\n", (i32)(v == c), (i32)(v != c))
    v.clear()
    printf("after_clear=%d\n", (i32)v.len())
    return 0
}
