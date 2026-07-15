//@ expect val 20
fn c()u32{u32[3] a={10,20,30} u32* q=&a[1] return *q} const u32 X=c() fn main()i32{return (i32)X}
