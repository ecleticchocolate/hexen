//@ expect val 15
struct V{f32 x} fn c()f32{V v={.x=1.5} return v.x} const f32 R=c() fn main()i32{return (i32)(R*10.0)}
