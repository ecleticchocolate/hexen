//@ expect val 28
fn f(i32 a,i32 b,i32 c,i32 d,i32 e,i32 g,i32* h)i32{return a+b+c+d+e+g+*h} fn main()i32{i32 x=7 return f(1,2,3,4,5,6,&x)}
