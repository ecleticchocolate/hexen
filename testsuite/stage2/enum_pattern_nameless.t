//@ expect val 11
// Variant names are identity in a DECLARATION but meaningless in a PATTERN,
// exactly like struct field names -- matching is positional either way.
enum E1 { i32 Ok  u8 Bad }
enum E2 { i32 Ok  u8 Nope }
fn p[T]() i32 { match T { enum { i32; u8 } { return 1 } else { return 2 } } return -1 }
fn main() i32 { return p[E1]()*10 + p[E2]() }
