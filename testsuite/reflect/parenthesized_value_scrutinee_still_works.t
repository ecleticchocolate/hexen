//@ expect val 5
enum E { i32 A  None }
fn main() i32 { E e = .A(5)  match (e) { .A(x) { return x } .None { return 0 } } }
