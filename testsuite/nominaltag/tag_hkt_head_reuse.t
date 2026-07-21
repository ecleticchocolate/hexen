//@ expect val 1
// The bound head stays an UNAPPLIED template, so it can be re-applied in the body.
struct Box[T] { T v }
fn p[S]() i32 {
    match S { struct M[X] { M[u8] q  return (i32)sizeof(q) } else { return 2 } }
    return -1
}
fn main() i32 { return p[Box[i32]]() }
