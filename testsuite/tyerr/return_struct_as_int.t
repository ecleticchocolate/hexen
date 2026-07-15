//@ expect err cannot assign
struct P{i32 a} fn f()i32{P p={.a=1} return p} fn main()i32{return f()}
