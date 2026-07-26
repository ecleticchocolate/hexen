//@ use std/option.t std/iterator.t std/vector.t std/array.t std/hashmap.t
//@ expect stdout
//@ | new=1 dup=0
//@ | get=2
//@ | has=1 missing=0
//@ | ptr_write=55
//@ | removed=55 len=0
extern fn printf(u8* fmt, ...) i32
fn main() i32 {
    HashMap[i32, i32] m = HashMap[i32, i32].create()
    printf("new=%d dup=%d\n", (i32)m.insert(10, 1), (i32)m.insert(10, 2))
    match m.get(10) { {.Some = x} { printf("get=%d\n", x) }  .None {} }
    printf("has=%d missing=%d\n", (i32)m.contains(10), (i32)m.contains(99))
    match m.get_ptr(10) { {.Some = p} { *p = 55 }  .None {} }
    printf("ptr_write=%d\n", m[10])
    match m.remove(10) { {.Some = x} { printf("removed=%d len=%d\n", x, (i32)m.len()) }  .None {} }
    return 0
}
