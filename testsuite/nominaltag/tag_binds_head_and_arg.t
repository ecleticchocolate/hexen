//@ expect val 4
// `struct M[X]`: tag makes M readable as a nominal head; X binds to the type arg.
struct Box[T] { T v }
fn p[S]() i32 {
    match S { struct M[X] { X v = 0  return (i32)sizeof(v) } else { return 2 } }
    return -1
}
fn main() i32 { return p[Box[i32]]() }
