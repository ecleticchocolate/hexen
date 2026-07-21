//@ expect val 3
fn n[T]() u32 { match T { struct {} { return 0 }  struct { A; Rest... } { return 1 + n[Rest]() } } return 99 }
fn main() i32 { return (i32)n[struct{i32 a  u8 b  u16 c}]() }
