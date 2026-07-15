//@ expect val 0
struct Point { f32 x  f32 y }
struct Box[T] { T value }
fn make_zero[T](u32 seed) T { return (T){} }
fn wrap[T](T v) Box[T] {
    Box[T] r
    r.value = v
    return r
}
fn consume(Box[Point] b) i32 { return (i32)b.value.x }
fn main() i32 { return consume(wrap(make_zero(99))) }
