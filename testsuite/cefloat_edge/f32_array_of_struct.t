//@ expect val 40
struct P{f32 x} fn c()f32{P[2] a={{.x=1.5},{.x=2.5}} return a[0].x+a[1].x} const f32 R=c() fn main()i32{return (i32)(R*10.0)}
