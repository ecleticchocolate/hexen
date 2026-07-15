//@ expect val 55
struct V{f32 x f32 y} fn c()f32{V v={.x=1.0,.y=2.0} v.x=3.5 return v.x+v.y} const f32 R=c() fn main()i32{return (i32)(R*10.0)}
