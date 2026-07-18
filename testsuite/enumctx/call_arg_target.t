//@ expect val 105
enum Ev { u32 A  u32 B  None }
fn h(Ev e) i32 { match e { .A(v) { return (i32) v }  .B(v) { return 100 + (i32) v }  .None { return -1 } } return -2 }
fn main() i32 { return h(.B(5)) }
