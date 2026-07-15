//@ expect err not a constant expression
struct Plain { u32 x } impl Plain { fn get() u32 { return 7 } } fn main() i32 { Plain p = {.x = 1} const u32 c = p.get() return (i32)c }
