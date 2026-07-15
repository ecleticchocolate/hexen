//@ expect val 3
struct P { u32 x  u32 y }
fn id[T](T v) T { return v }
fn main() i32 {
    P p = id((P){.x = 1, .y = 2})
    return (i32)(p.x + p.y)
}
