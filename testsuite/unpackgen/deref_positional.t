//@ expect val 12
struct P { i32 x  i32 y }
fn main() i32 { P p={3,9}  P* pp=&p  unpack { a, b } = pp  return a+b }
