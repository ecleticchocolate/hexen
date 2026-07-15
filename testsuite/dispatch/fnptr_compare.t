//@ expect val 10
fn f(i32 x)i32{return x}
fn g(i32 x)i32{return x+1}
fn main()i32{
    fn(i32)i32 a=f fn(i32)i32 b=f fn(i32)i32 c=g
    return (i32)(a==b)*10+(i32)(a==c)
}
