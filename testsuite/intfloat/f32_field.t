//@ expect val 30
struct V{f32 x} fn main()i32{V v={.x=3} return (i32)(v.x*10.0)}
