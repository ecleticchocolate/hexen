//@ expect val 35
struct Vec{u32 x u32 y} fn mk_a(u32 n)Vec{return {.x=n,.y=n*2}} fn mk_b(u32 n)Vec{return {.x=n*3,.y=n}} struct Factory{(fn(u32)Vec)[2] makers} fn main()i32{Factory f={.makers={mk_a,mk_b}} Vec v0=f.makers[0](5) Vec v1=f.makers[1](5) return (i32)(v0.x+v0.y+v1.x+v1.y)}
