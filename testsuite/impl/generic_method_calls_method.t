//@ expect val 42
struct Box[T] { T val }
impl Box[T] {
    fn get() T { return self.val }
    fn double_get() T { return self.get() }
}
fn main() i32 {
    Box[i32] b = {.val = 21}
    return b.double_get() * 2
}
