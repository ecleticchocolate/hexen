//@ expect val 49
struct Node { u32 key  Node* left }
fn build() u32 {
    Node* root = new Node{.key = 42, .left = null}
    Node* child = new Node{.key = 7, .left = null}
    root.left = child
    return root.key + root.left.key
}
const u32 TOTAL = build()
fn main() i32 { return (i32)TOTAL }
