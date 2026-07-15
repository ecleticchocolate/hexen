//@ expect val 42
struct P { u32 x  u32 y }
fn add2(P a, P b) u32 { return a.x + a.y + b.x + b.y }
fn main() i32 { return (i32) add2({.x = 10, .y = 20}, {.x = 3, .y = 9}) }
