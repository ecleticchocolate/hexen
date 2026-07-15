//@ expect val 620
struct Ops{ fn(i32)i32 f  i32 bias }
fn inc(i32 x)i32{return x+1}
fn dbl(i32 x)i32{return x*2}
fn apply(Ops o,i32 x)i32{return o.f(x)+o.bias}
fn main()i32{
    Ops a={.f=inc,.bias=0}
    Ops b={.f=dbl,.bias=10}
    return apply(a,5)*100+apply(b,5)
}
