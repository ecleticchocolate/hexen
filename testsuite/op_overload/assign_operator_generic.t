//@ expect val 42
struct Box[T] { T v }
impl Box[T] {
    fn __assign(T scalar) void { self.v = scalar }
}
fn main() i32 {
    Box[i32] b = { .v = 0 }
    b = 42
    return b.v
}
