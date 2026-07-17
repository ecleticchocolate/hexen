//@ expect val 21
fn main() i32 { i32 a=1  i32 b=2  struct{i32 i32} t={b,a}  unpack {a,b}=t  return a*10+b }
