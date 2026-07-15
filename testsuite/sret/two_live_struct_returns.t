//@ expect val 30
// Two aggregate-returning calls whose results are BOTH live as arguments to a
// third call. backend_x64.c used to give every sret call in a function the same
// hardcoded scratch slot (`lea rbx, [rbp-1024]`), so the second call's result
// overwrote the first's and `add(mk(10), mk(20))` returned 40 (20+20), not 30.
struct Box { u32 v }
fn mk(u32 x) Box { return {.v = x} }
fn add(Box a, Box b) u32 { return a.v + b.v }
fn main() i32 { return (i32) add(mk(10), mk(20)) }
