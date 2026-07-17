//@ expect val 45
// Axis cross: generic Cursor (specialize_return fix) + new/pointer-chain
// traversal + for-in over a dereferenced pointer receiver (*head) + const
// (compile-time interpreter). Mirrors REFERENCE.md's own comptime linked-
// list showcase, but iterating the list via the cursor protocol instead of
// a manual while/.next walk.
enum Option[T] { T Some  None }
struct Node[T] { T val  Node[T]* next }
struct Cur[T] { Node[T]* p }
impl Node[T] { fn begin() Cur[T] { return {.p = self} } }
impl Cur[T] {
    fn next() Option[T] {
        if self.p == null { return .None }
        T v = self.p.val
        self.p = self.p.next
        return .Some{v}
    }
}
fn build(u32 n) Node[i32]* {
    Node[i32]* head = null
    for u32 i = 0 to n {
        head = new Node[i32]{.val = (i32)i, .next = head}
    }
    return head
}
fn sumall(Node[i32]* head) i32 {
    i32 total = 0
    for i32 v in *head { total = total + v }
    return total
}
const i32 RESULT = sumall(build(10))
fn main() i32 { return RESULT }
