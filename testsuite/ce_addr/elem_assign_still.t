//@ expect val 99
fn c()u32{u32[3] a={1,2,3} a[0]=99 return a[0]} const u32 X=c() fn main()i32{return (i32)X}
