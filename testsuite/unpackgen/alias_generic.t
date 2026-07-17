//@ expect val 12
alias Box[T] = struct { T a  T b }
fn main() i32 { Box[i32] b={4,8}  unpack {x,y}=b  return x+y }
