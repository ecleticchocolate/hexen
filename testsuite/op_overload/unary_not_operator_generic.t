//@ expect val 0
struct Box[T] { T v }
impl Box[T] {
    fn __not() bool { return self.v == 0 }
}
fn main() i32 {
    Box[i32] a = { .v = 0 }
    Box[i32] b = { .v = 7 }
    if (!a) {
        if (!(!b)) { return 0 }
    }
    return 1
}
