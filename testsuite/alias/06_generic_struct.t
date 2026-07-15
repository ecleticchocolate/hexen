//@ expect val 7
alias Pair[T] = struct { T a  T b }
fn main() i32 { Pair[i32] p = { .a = 3, .b = 4 }  return p.a + p.b }
