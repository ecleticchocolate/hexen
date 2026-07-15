//@ expect val 0
struct Point{f32 x f32 y}
struct Box[T]{T value}
fn make[T](u32 seed) T { return (T){} }
fn wrap[T](T v) Box[T] { return {.value=v} }
fn consume(Box[Point] b) i32 { return (i32)b.value.x }
fn main() i32 { return consume(wrap(make(99))) }
