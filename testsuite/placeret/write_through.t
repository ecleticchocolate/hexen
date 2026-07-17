//@ expect val 99
fn main() i32 { i32 a=0; i32* p = &(a = 7); *p = 99; return a }
