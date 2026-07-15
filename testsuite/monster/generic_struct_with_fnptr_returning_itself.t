//@ expect val 99
struct Box[T] { T val }
struct Node { fn() Box[u32] get_box }
fn make_box() Box[u32] { return {.val = 99} }
fn main() i32 {
    Node n = {.get_box = make_box}
    return (i32) n.get_box().val
}
