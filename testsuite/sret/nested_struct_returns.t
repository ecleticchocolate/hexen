//@ expect val 7
// General coverage: a struct-returning call whose argument is itself a
// struct-returning call. (Note: this does NOT catch the rbp-1024 sret-slot bug --
// verified against the buggy compiler, it passes. two_live_struct_returns.t is
// the one that bites.)
struct Box { u32 v }
fn mk(u32 x) Box { return {.v = x} }
fn bump(Box b) Box { return {.v = b.v + 1} }
fn main() i32 { return (i32) bump(bump(mk(5))).v }
