//@ expect val 12
// A TAGGED head is nominal only: it must not match an array.
struct Box[T] { T v }
fn p[S]() i32 { match S { struct M[X] { return 1 } else { return 2 } } return -1 }
fn main() i32 { return p[Box[i32]]()*10 + p[i32[4]]() }
