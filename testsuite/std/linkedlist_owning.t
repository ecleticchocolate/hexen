//@ use std/option.t std/iterator.t std/vector.t std/linkedlist.t
//@ expect stdout
//@ | len=3
//@ | elem=1 v0=11
//@ | deep_copy=1
//@ | after_set=1
//@ | popped=1
//@ | survived
// An owning T is deep-copied in (store_elem), moved out on removal (move_out,
// which blanks the node so `delete n` cannot free what the caller now owns),
// and destroyed exactly once at teardown.
extern fn printf(u8* fmt, ...) i32
fn mk(u8 v) Vector[u8] { Vector[u8] i = Vector[u8].create()  i.push(v)  return i }
fn main() i32 {
    LinkedList[Vector[u8]] l = LinkedList[Vector[u8]].create()
    for u32 i = 0 to 3 { l.push_back(mk((u8)(10 + i))) }
    printf("len=%d\n", (i32)l.len())
    match l.get(1) { {.Some = *p} { printf("elem=%d v0=%d\n", (i32)p.len(), (i32)(*p)[0]) } .None {} }
    LinkedList[Vector[u8]] c = l.copy()
    printf("deep_copy=%d\n", (i32)(l == c))
    printf("after_set=%d\n", (i32)l.set(0, mk(99)))
    match l.pop_front() { {.Some = *p} { printf("popped=%d\n", (i32)p.len()) } .None {} }
    l.clear()
    printf("survived\n")
    return 0
}
