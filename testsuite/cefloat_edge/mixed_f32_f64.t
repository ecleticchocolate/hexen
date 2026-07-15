//@ expect val 40
struct M{f32 a f64 b} fn c()f64{M m={.a=1.5,.b=2.5} return (f64)m.a+m.b} const f64 R=c() fn main()i32{return (i32)(R*10.0)}
