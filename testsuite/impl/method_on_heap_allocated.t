//@ expect val 42
struct Node { i32 val }
impl Node {
    fn get() i32 { return self.val }
    fn set(i32 v) { self.val = v }
}
fn main() i32 {
    Node* n = new Node{.val = 7}
    n.set(42)
    i32 v = n.get()
    delete n
    return v
}
