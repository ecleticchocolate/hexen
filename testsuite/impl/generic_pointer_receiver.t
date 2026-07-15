//@ expect val 77
struct Box[T] { T val }
impl Box[T] {
    fn set(T v) { self.val = v }
    fn get() T { return self.val }
}
fn main() i32 {
    Box[i32] b = {.val = 0}
    Box[i32]* p = &b
    p.set(77)
    return p.get()
}
