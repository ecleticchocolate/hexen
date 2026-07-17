//@ expect val 12
struct P { i32 x  i32 y }
struct Q { i32 a  i32 b }
fn main() i32 { P p={3,9}  Q q  unpack { .x=q.a, .y=q.b } = p  return q.a+q.b }
