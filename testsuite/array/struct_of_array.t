//@ expect val 35
struct S { u32[3] arr }
fn main() i32 { S s = {.arr = {10, 20, 30}}; s.arr[0] = 5; return (i32)(s.arr[0] + s.arr[2]) }
