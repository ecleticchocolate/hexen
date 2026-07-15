//@ expect val 30
struct Box[T] { T val }
impl Box[T] {
    fn get() T { return self.val }
}
fn main() i32 {
    Box[i32] bi = {.val = 10}
    Box[u32] bu = {.val = 20}
    return bi.get() + (i32)bu.get()
}
