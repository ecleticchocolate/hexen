//@ expect val 42
struct P { u32 x  u32 y }
fn sum(P p) u32 { return p.x + p.y }
fn main() i32 { return (i32) sum({.x = 40, .y = 2}) }
