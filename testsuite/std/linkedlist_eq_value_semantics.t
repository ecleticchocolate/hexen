//@ use std/option.t std/iterator.t std/vector.t std/linkedlist.t
//@ expect stdout
//@ | plain eq=1 neq=0
//@ | owning eq=1 neq=0
//@ | differs=0
//@ | both alive: 2 2
//@ | survived
// `a == b` takes its operand BY VALUE, so the call site is plain `a == b` with
// no `&`. The operand is a byte copy that aliases the same nodes -- safe here
// because __eq only READS, and because a by-value parameter is not destroyed on
// return (the caller's original stays the sole owner).
extern fn printf(u8* fmt, ...) i32
fn mk(u8 v) Vector[u8] { Vector[u8] i = Vector[u8].create()  i.push(v)  return i }
fn main() i32 {
    LinkedList[i32] a = LinkedList[i32].create()
    LinkedList[i32] b = LinkedList[i32].create()
    a.push_back(1)  a.push_back(2)
    b.push_back(1)  b.push_back(2)
    printf("plain eq=%d neq=%d\n", (i32)(a == b), (i32)(a != b))

    LinkedList[Vector[u8]] x = LinkedList[Vector[u8]].create()
    LinkedList[Vector[u8]] y = LinkedList[Vector[u8]].create()
    x.push_back(mk(7))  x.push_back(mk(8))
    y.push_back(mk(7))  y.push_back(mk(8))
    printf("owning eq=%d neq=%d\n", (i32)(x == y), (i32)(x != y))
    y.push_back(mk(9))
    printf("differs=%d\n", (i32)(x == y))
    // both operands still usable after the comparison
    printf("both alive: %d %d\n", (i32)x.len(), (i32)y.len() - 1)
    printf("survived\n")
    return 0
}
