//@ use std/option.t std/iterator.t std/vector.t std/linkedlist.t
//@ expect stdout
//@ | plain sum=60
//@ | write-through sum=600
//@ | owning lens=6
//@ | empty n=0
// `for in` walks node-to-node via `cur.next` -- O(1) per step, O(n) total, with
// no index lookup. Measured: 100k/200k/400k elements take 0.00/0.01/0.02 s.
extern fn printf(u8* fmt, ...) i32
fn mk(u8 v) Vector[u8] { Vector[u8] i = Vector[u8].create()  i.push(v)  i.push(v)  return i }
fn main() i32 {
    LinkedList[i32] l = LinkedList[i32].create()
    l.push_back(10)  l.push_back(20)  l.push_back(30)
    i32 s = 0
    for i32* p in l { s = s + *p }
    printf("plain sum=%d\n", s)
    // write-through: the iterator yields a pointer into the live node
    for i32* p in l { *p = *p * 10 }
    i32 s2 = 0
    for i32* p in l { s2 = s2 + *p }
    printf("write-through sum=%d\n", s2)
    // owning elements
    LinkedList[Vector[u8]] o = LinkedList[Vector[u8]].create()
    o.push_back(mk(1))  o.push_back(mk(2))  o.push_back(mk(3))
    i32 total = 0
    for Vector[u8]* v in o { total = total + (i32)v.len() }
    printf("owning lens=%d\n", total)
    // empty list terminates immediately
    LinkedList[i32] e = LinkedList[i32].create()
    i32 n = 0
    for i32* p in e { n = n + 1 }
    printf("empty n=%d\n", n)
    return 0
}
