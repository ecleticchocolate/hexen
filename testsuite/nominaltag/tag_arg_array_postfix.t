//@ expect val 1
struct Box[T] { T v }
fn p[T]() i32 { match T { struct M[E[N]] { return 1 } else { return 2 } } return -1 }
fn main() i32 { return p[Box[u8[4]]]() }
