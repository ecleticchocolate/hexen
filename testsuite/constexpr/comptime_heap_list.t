//@ expect val 15
struct Node { u32 val  Node* next }
fn build() u32 {
    Node* head = null
    u32 i = 5
    while i >= 1 {
        head = new Node{.val = i, .next = head}   // prepend on comptime heap
        if i == 1 { i = 0 } else { i = i - 1 }
    }
    u32 sum = 0
    Node* cur = head
    while cur != null { sum = sum + cur.val  cur = cur.next }   // walk pointers
    return sum
}
const u32 SUM = build()   // 1+2+3+4+5 = 15
fn main() i32 { return (i32)SUM }
