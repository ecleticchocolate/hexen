//@ expect err not a constant expression
struct P { u32 x  u32 y }
fn divp(P a, P b) P { return a / b }
const P bad = divp({.x = 10, .y = 10}, {.x = 2, .y = 2})
fn main() u32 { return bad.x }
