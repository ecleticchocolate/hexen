//@ expect val 10
fn main() i32 { i32 a=0; i32 b=5; i32* p=&(a=b); return a + *p }
