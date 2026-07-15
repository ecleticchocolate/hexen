//@ expect val 9
struct Box[T] { T value }
fn make[T](u32 seed) T { return (T) 9.0 }
fn wrap[T](T v) Box[T] { return {.value = v} }
struct Holder { Box[f32] b }
fn main() i32 { Holder h = {.b = wrap(make(1))} return (i32)h.b.value }
