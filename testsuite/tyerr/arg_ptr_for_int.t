//@ expect err cannot assign
fn f(i32 x)i32{return x} fn main()i32{i32 y=1 return f(&y)}
