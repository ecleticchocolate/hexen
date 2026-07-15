//@ expect val 15
struct I{f32 v} struct O{I i} fn c()f32{O o={.i={.v=1.5}} return o.i.v} const f32 R=c() fn main()i32{return (i32)(R*10.0)}
