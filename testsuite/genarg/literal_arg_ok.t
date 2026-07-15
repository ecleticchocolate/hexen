//@ expect val 10
struct Foo[u32 N] { i32[N] arr } fn main() i32 { Foo[10] f return (i32)(sizeof(f.arr)/sizeof(i32)) }
