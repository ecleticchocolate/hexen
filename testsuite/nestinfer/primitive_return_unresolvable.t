//@ expect val 1
fn zero_of[T](u32 seed) T { return (T) 0 }
fn wants_i64(i64 x) i64 { return x + 1 }
fn main() i32 { return (i32)wants_i64(zero_of(7)) }
