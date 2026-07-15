//@ expect val 42
struct Box[T] { T val }
impl Box[T,U] {
    fn get() T { return self.val }
}
fn main() i32 {
    Box[i32] b = {.val = 42}
    return b.get()
}
