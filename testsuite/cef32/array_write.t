//@ expect val 55
fn c()f32{f32[2] a={1.0,2.0} a[0]=3.5 return a[0]+a[1]} const f32 R=c() fn main()i32{return (i32)(R*10.0)}
