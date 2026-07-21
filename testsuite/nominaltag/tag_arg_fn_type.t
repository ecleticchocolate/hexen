//@ expect val 1
struct Box[T] { T v }
fn p[T]() i32 { match T { struct M[fn(A) B] { return 1 } else { return 2 } } return -1 }
fn main() i32 { return p[Box[fn(u8) u16]]() }
