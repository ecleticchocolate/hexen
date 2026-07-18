//@ expect val 99
enum E { u32 A  None }
fn f() u32 { E e = .None  match e { .A(v) { return v }  .None { return 99 } }  return 0 }
const u32 X = f()
fn main() i32 { return (i32) X }
