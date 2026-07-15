//@ expect val 57
struct Box[T] { T val }
impl Box[T] {
    fn get() T { return self.val }
    fn set(T v) { self.val = v }
}
fn main() i32 {
    Box[i32] b = {.val = 42}
    i32 v = b.get()
    b.set(99)
    return b.get() - v
}
