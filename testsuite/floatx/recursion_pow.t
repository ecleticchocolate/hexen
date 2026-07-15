//@ expect val 8
fn pw(f64 b,u32 n)f64{if n==0{return 1.0} return b*pw(b,n-1)} fn main()i32{return (i32)pw(2.0,3)}
