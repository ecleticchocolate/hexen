//@ expect val 99
struct S { i32 x = 99 }
fn main() i32 { S s = {} return s.x }
