//@ expect val 555
struct Box[T] { T val }
impl Box[T] {
    fn get() T { return self.val }
}
fn main() i32 {
    Box[Box[i32]] bb = {.val = {.val = 555}}
    return bb.get().get()
}
