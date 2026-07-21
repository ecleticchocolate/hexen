//@ expect val 8
enum Opt[T] { T Some  None }
fn p[S]() i32 { match S { enum M[X] { X v = 0  return (i32)sizeof(v) } else { return 2 } } return -1 }
fn main() i32 { return p[Opt[u64]]() }
