//@ expect val 20
struct A{i32 x i32 y} struct B{A a A b i32 c} fn main()i32{return (i32)sizeof(B)}
