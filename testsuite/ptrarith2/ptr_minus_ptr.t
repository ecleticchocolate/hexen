//@ expect val 4
fn main()i32{ i32[5] a={0,0,0,0,0} i32* p=&a[0] i32* q=&a[4] return (i32)(q-p) }
