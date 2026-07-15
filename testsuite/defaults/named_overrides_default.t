//@ expect val 6
struct S { i32 x = 99  i32 y = 1 }
fn main() i32 { S s = {.x = 5} return s.x + s.y }
