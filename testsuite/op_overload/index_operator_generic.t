//@ expect val 42
struct Box[T] { T v }
impl Box[T] {
    fn __index(i32 i) T { return self.v }
}
fn main() i32 {
    Box[i32] b = { .v = 42 }
    return b[0]
}
