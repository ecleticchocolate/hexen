//@ expect val 12
struct P { i32 x  i32 y }
alias PP = P*
fn main() i32 { P p={3,9}  PP ptr=&p  unpack { .x=a, .y=b } = ptr  return a+b }
