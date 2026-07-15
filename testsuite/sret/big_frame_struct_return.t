//@ expect val 123
// General coverage: a struct return in a function whose frame exceeds 1KB.
// (Note: does NOT catch the rbp-1024 sret-slot bug in practice -- the old
// hardcoded slot happened to miss `guard`. Kept as frame-layout coverage.)
struct Box { u32 v }
fn mk(u32 x) Box { return {.v = x} }
fn main() i32 {
    u8[1200] pad
    u32 guard = 123
    pad[0] = 1
    Box b = mk(7)
    return (i32) (guard + b.v - b.v)
}
