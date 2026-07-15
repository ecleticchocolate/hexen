//@ expect err no runtime address
struct Node { u32 val  Node* next }
struct List { Node* head  u32 len }
fn build() List {
    Node* h = new Node{.val = 99, .next = null}
    return {.head = h, .len = 1}
}
const List L = build()          // stores a comptime-heap pointer -> rejected
fn main() u32 { return L.head.val }
