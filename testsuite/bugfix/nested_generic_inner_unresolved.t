//@ expect val 42
struct Box[T] { T value }
fn make[T](u32 seed) T { return (T)seed }
fn wrap[T](T v) Box[T] {
    Box[T] r
    r.value = v
    return r
}
fn consume(Box[i32] b) i32 { return b.value }
fn main() i32 { return consume(wrap(make(42))) }
