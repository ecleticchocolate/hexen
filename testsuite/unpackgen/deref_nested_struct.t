//@ expect val 12
struct Inner { i32 v }
struct Outer { Inner in  i32 w }
fn main() i32 { Outer o={{7},5}  Outer* op=&o  unpack { .in={.v=a}, .w=b } = op  return a+b }
