//@ expect val 1430
struct P { u32 x  u32 y }
fn subp(P a, P b) P { return a - b }
fn mulp(P a, P b) P { return a * b }
const P diff = subp({.x = 10, .y = 20}, {.x = 3, .y = 5})
const P prod = mulp(diff, {.x = 2, .y = 2})
fn main() u32 { return prod.x * 100 + prod.y }
