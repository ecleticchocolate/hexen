//@ expect val 1
fn main()i32{ i32[3] a={1,2,3} i32* p=&a[0] i32* q=&a[2] return (i32)(p<q) }
