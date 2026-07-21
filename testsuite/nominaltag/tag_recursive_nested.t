//@ expect val 8
// Tagged applications nest: the argument of one is another tagged application,
// and the inner wildcard binds normally. Ordinary reflect_unify recursion.
struct Box[T] { T v }
struct Wrap[T] { T v }
fn p[S]() i32 {
    match S { struct M[struct N[X]] { X v = 0  return (i32)sizeof(v) } else { return 2 } }
    return -1
}
fn main() i32 { return p[Box[Wrap[u64]]]() }
