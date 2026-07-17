//@ expect val 102
fn main() i32 { i32 a=1; i32 b=2; i32* p=&(a=b); *p = *p + 100; return a }
