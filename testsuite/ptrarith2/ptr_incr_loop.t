//@ expect val 26
fn main()i32{ i32[4] a={5,6,7,8} i32* p=&a[0] i32 s=0 for i32 i=0 to 4{s=s+*p p=p+1} return s }
