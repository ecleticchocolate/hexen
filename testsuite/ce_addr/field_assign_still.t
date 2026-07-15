//@ expect val 42
struct P{u32 a u32 b} fn c()u32{P p={.a=1,.b=2} p.a=40 return p.a+p.b} const u32 X=c() fn main()i32{return (i32)X}
