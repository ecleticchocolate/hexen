//@ expect val 47
struct Box[T] { T val }
impl Box[T] {
    fn get() T { return self.val }
    fn map[U](U v) U { return v }
}
fn main() i32 {
    Box[i32] b = {.val = 42}
    i32 x = b.map(5)
    return b.get() + x
}
