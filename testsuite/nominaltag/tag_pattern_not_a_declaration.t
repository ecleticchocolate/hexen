//@ expect val 4
// `struct M[X] {` inside a function is a match ARM, lexically identical to a
// top-level declaration. Pass 0 must not register M as a real struct, or the
// wildcard stops being a wildcard.
struct Box[T] { T v }
fn p[S]() i32 {
    match S { struct M[X] { X v = 0  return (i32)sizeof(v) } else { return 2 } }
    return -1
}
fn main() i32 { return p[Box[i32]]() }
