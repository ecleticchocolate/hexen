//@ expect val 12
// Arity is read off the CONCRETE instantiation, so a 1-bracket pattern
// does not match a 2-arg template.
struct Box[T] { T v }
struct Pair[A,B] { A a  B b }
fn p[S]() i32 { match S { struct M[X] { return 1 } else { return 2 } } return -1 }
fn main() i32 { return p[Box[i32]]()*10 + p[Pair[i32,u8]]() }
