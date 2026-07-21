//@ expect val 12
// Variant names assert in a pattern exactly as struct field names do.
enum E1 { i32 Ok }
enum E2 { i32 Nope }
fn p[T]() i32 { match T { enum { i32 Ok } { return 1 } else { return 2 } } return -1 }
fn main() i32 { return p[E1]()*10 + p[E2]() }
