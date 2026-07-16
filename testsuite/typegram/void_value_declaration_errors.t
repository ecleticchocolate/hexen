//@ expect err a type cannot be used as a value
fn main() i32 { void v = void  return (i32)sizeof(v) }
