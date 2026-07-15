//@ expect val 42
fn sum(struct { i32 a  i32 b } p) i32 { return (i32)(p.a + p.b) }
fn main() i32 { struct { i32 a  i32 b } v  v.a = 40  v.b = 2  return sum(v) }
