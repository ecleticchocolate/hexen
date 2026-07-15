//@ expect val 6
fn identity[T](T x) T { return x }
fn takes_u32(u32 x) u32 { return x + 1 }
fn main() i32 { return (i32)takes_u32(identity(5)) }
