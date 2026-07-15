//@ expect val 500500
struct P { u32 x  u32 y }
fn sum(P p) u32 { return p.x + p.y }
fn main() i32 {
    u32 acc = 0
    for u32 i = 0 to 1000 { acc += sum({.x = i, .y = 1}) }
    return (i32) acc
}
