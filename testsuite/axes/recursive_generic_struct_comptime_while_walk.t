//@ expect val 45
// Recursive generic struct (self-ref via pointer) traversed and summed
// entirely inside a `const` initializer -- the SAME interpreter claim from
// REFERENCE.md's own linked-list showcase, but with the node type itself
// generic (Node[T]) rather than a fixed concrete struct, and using a plain
// while/.next walk (not the cursor protocol) to isolate JUST the
// comptime + generic-recursive-struct interaction.
struct Node[T] { T val  Node[T]* next }
fn build(u32 n) Node[i32]* {
    Node[i32]* head = null
    for u32 i = 0 to n {
        head = new Node[i32]{.val = (i32)i, .next = head}
    }
    return head
}
fn sum(Node[i32]* head) i32 {
    i32 total = 0
    Node[i32]* cur = head
    while cur != null {
        total = total + cur.val
        cur = cur.next
    }
    return total
}
const i32 RESULT = sum(build(10))
fn main() i32 { return RESULT }
