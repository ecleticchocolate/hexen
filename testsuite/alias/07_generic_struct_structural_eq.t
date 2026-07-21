//@ expect val 30
alias Pair[T] = struct { T a  T b }
fn mk() Pair[i32] { return { .a = 10, .b = 20 } }
fn main() i32 { Pair[i32] p = mk()  return p.a + p.b }
