//@ expect val 41
// `struct M[X,Y]` -- the ordinary comma-separated arg list. Equivalent to the
// stacked `struct M[X][Y]`; the tagged form has no grammar of its own.
struct Pair[A,B] { A a  B b }
fn p[S]() i32 {
    match S { struct M[X,Y] { X u = 0  Y v = 0  return (i32)sizeof(u)*10 + (i32)sizeof(v) } else { return 2 } }
    return -1
}
fn main() i32 { return p[Pair[i32,u8]]() }
