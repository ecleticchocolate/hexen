//@ expect val 55
struct Box[T] { T val }
fn main() i32 {
    Box[u32] b = {.val = 55}
    Box[u32]* p = &b
    Box[u32]** pp = &p
    return (i32) (*pp).val
}
