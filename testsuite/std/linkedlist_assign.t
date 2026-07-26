//@ use std/option.t std/iterator.t std/vector.t std/linkedlist.t
//@ expect stdout
//@ | decl-init: len=3 first=1 last=3
//@ | reassign: len=2 first=9
//@ | owning: len=2 inner=1
extern fn printf(u8* fmt, ...) i32
fn mk(u8 v) Vector[u8] { Vector[u8] i = Vector[u8].create()  i.push(v)  return i }
fn main() i32 {
    LinkedList[i32] l = {1, 2, 3}
    printf("decl-init: len=%d first=%d last=%d\n", (i32)l.len(), l[0], l[2])
    l = {9, 8}
    printf("reassign: len=%d first=%d\n", (i32)l.len(), l[0])
    LinkedList[Vector[u8]] o = LinkedList[Vector[u8]].create()
    o = {mk(1), mk(2)}
    match o.get(0) { {.Some = *p} { printf("owning: len=%d inner=%d\n", (i32)o.len(), (i32)p.len()) } .None {} }
    return 0
}
