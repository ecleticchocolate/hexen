//@ expect val 42
struct Cb { fn(u32) u32 f }
fn inc(u32 x) u32 { return x + 1 }
fn main() i32 { Cb c = {.f = inc}; return (i32) c.f(41) }
