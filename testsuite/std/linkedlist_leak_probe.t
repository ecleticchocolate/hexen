//@ use std/option.t std/iterator.t std/vector.t std/linkedlist.t
//@ expect val 0
fn mk(u8 v) Vector[u8] { Vector[u8] i = Vector[u8].create()  i.push(v)  return i }
fn main() i32 {
    LinkedList[Vector[u8]] l = LinkedList[Vector[u8]].create()
    Vector[u8] a = mk(1)
    l.push_back(a)
    l.clear()
    return 0
}
