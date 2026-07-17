//@ expect val 50
struct Vec { i32[4] d }
impl Vec { fn __index(u32 i) i32* { return &self.d[i] }  fn len() u64 { return 4 } }
fn main() i32 { Vec v  v.d={5,10,15,20}  i32 s=0  for i32 x in v { s=s+x }  return s }
