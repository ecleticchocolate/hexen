//@ expect val 42
struct P { u32 x  u32 y }
fn id[T](T v) T { return v }
fn main() i32 {
    P src = {.x = 40, .y = 2}
    P p = id(src)
    return (i32)(p.x + p.y)
}
