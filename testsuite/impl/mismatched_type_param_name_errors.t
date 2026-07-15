//@ expect err Expected return type or '{' in impl method
struct Box[T] { T val }
impl Box[X] {
    fn get() X { return self.val }
}
fn main() i32 {
    Box[i32] b = {.val = 42}
    return b.get()
}
