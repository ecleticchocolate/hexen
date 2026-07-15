//@ expect val 25
fn a(i32 x)i32{return x+1} fn b(i32 x)i32{return x*2} fn c(i32 x)i32{return x-3} fn d(i32 x)i32{return x*x} fn main()i32{return d(c(b(a(3))))}
