//@ expect val 12
// Postfix binds to the whole tagged application, as it does for a concrete head.
struct Box[T] { T v }
fn p[T]() i32 { match T { struct M[E]* { return 1 }  struct M[E] { return 2 }  else { return 3 } } return -1 }
fn main() i32 { return p[Box[u8]*]()*10 + p[Box[u8]]() }
