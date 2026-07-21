//@ expect val 12
// A trailing name in a PATTERN asserts the concrete field's name -- the only
// meaning it can have, since patterns match positionally and have no binding
// site. It used to be silently discarded, which hid typos.
struct S { i32 a }
struct W { i32 zzz }
fn p[T]() i32 { match T { struct { i32 a } { return 1 } else { return 2 } } return -1 }
fn main() i32 { return p[S]()*10 + p[W]() }
