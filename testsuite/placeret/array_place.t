//@ expect val 50
fn main() i32 { i32[3] a={0,0,0}; i32* p = &(a[1]=5); *p=50; return a[1] }
