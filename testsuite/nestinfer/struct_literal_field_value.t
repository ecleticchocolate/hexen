//@ expect val 7
fn make_f32[T](u32 seed) T { return (T) 7.0 }
struct Wrapper { f32 a }
fn main() i32 { Wrapper w = {.a = make_f32(1)} return (i32)w.a }
