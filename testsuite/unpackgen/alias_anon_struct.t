//@ expect val 12
alias Pair = struct { i32 a  i32 b }
fn main() i32 { Pair p={3,9}  unpack { .a=x, .b=y } = p  return x+y }
