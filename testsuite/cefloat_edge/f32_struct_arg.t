//@ expect val 25
struct P{f32 x} fn add(P p)f32{return p.x+1.0} fn c()f32{P p={.x=1.5} return add(p)} const f32 R=c() fn main()i32{return (i32)(R*10.0)}
