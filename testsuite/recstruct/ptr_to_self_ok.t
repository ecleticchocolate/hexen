//@ expect val 42
struct Node{Node* next i32 val} fn main()i32{Node n={.next=null,.val=42} return n.val}
