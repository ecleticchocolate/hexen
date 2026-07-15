//@ expect val 42
struct P{u32 a u32 b} fn main()i32{P p={.a=42,.b=0} u32* q=&p.a return (i32)*q}
