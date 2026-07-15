//@ expect val 7
struct Node { u32 val  Node* next }
const Node N = {.val = 7, .next = null}   // null pointer is fine in a const
fn main() u32 { return N.val }
