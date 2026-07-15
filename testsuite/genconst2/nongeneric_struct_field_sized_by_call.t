//@ expect val 8
fn width(u32 n) u32 { return n + 5 }
struct Buf { i32[width(3)] data }
fn main() i32 { return (i32)(sizeof(Buf)/sizeof(i32)) }
