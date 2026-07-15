//@ expect val 12
fn h(i32 x)i32{return x*2} fn g(i32 x)i32{return x+3} fn f(i32 x)i32{return x-1} fn main()i32{return f(g(h(5)))}
