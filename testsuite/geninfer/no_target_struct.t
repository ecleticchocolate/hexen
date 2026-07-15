//@ expect err cannot infer
struct P { u32 x  u32 y }
fn id[T](T v) T { return v }
fn sink[T](T v) i32 { return 0 }
fn main() i32 {
    return sink(id({.x = 1, .y = 2}))
}
