//@ expect val 30
struct P { u32 x  u32 y }
fn main() i32 {
    P[2] a = {{1, 2}, {10, 20}}
    return (i32)(a[1].x + a[1].y)
}
