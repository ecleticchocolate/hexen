//@ expect val 1
fn main()i32{ i32[3] a={1,2,3} i32* p=&a[2] i32* q=&a[0] return (i32)(p>q) }
