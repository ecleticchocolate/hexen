//@ expect val 60
struct Node[T] { T val  Node[T]* next }
fn push[T](Node[T]** h, T v) { Node[T]* n = new Node[T]; n.val = v; n.next = *h; *h = n }
fn sum[T](Node[T]* h) T { T a = h.val - h.val; Node[T]* c = h; while c != null { a = a + c.val; c = c.next } return a }
fn free_list[T](Node[T]* h) { while h != null { Node[T]* n = h.next; delete h; h = n } }
fn main() i32 {
    Node[i32]* h = null
    push(&h, 10); push(&h, 20); push(&h, 30)
    i32 r = sum(h)
    free_list(h)
    return r
}
