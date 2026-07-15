//@ expect err compile-time constant
struct Foo[u32 N] { i32[N] arr } fn main() i32 { u32 k = 999  Foo[k] f f.arr[0] = 1  return 0 }
