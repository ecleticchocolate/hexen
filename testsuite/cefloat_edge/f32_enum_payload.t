//@ expect val 15
enum E{f32 V None} fn c()f32{E e=.V(1.5) match e{.V(v) {return v} .None {return 0.0}} return 0.0} const f32 R=c() fn main()i32{return (i32)(R*10.0)}
