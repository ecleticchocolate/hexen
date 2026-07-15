//@ expect err compile-time constant
struct Foo[u32 N] { i32[N] arr } fn main() i32 { u32 k = 5  Foo[k * 2] f return (i32)(sizeof(f.arr)/sizeof(i32)) }
