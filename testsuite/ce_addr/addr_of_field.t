//@ expect val 42
struct P{u32 a u32 b} fn c()u32{P p={.a=42,.b=0} u32* q=&p.a return *q} const u32 X=c() fn main()i32{return (i32)X}
