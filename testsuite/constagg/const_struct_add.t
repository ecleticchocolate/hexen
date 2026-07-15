//@ expect val 1122
struct P { u32 x  u32 y }
fn addp(P a, P b) P { return a + b }
const P sum = addp({.x = 1, .y = 2}, {.x = 10, .y = 20})
fn main() u32 { return sum.x * 100 + sum.y }
