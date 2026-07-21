//@ expect val 12
// Slots are independent: assert one, stay positional on the next.
struct S { i32 a  u8 b }
struct W { i32 zzz  u8 b }
fn p[T]() i32 { match T { struct { i32 a; u8 } { return 1 } else { return 2 } } return -1 }
fn main() i32 { return p[S]()*10 + p[W]() }
