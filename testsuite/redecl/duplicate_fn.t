//@ expect err already declared
fn f() u32 { return 1 }  fn f() u32 { return 2 }  fn main() i32 { return (i32) f() }
