//@ expect val 21
struct P { i32 x  i32 y }
struct Bag { P[3] items }
impl Bag { fn __index(u32 i) P* { return &self.items[i] }  fn len() u64 { return 3 } }
fn main() i32 { Bag b  b.items={{1,2},{3,4},{5,6}}  i32 s=0  for unpack { .x=a, .y=c } in b { s=s+a+c }  return s }
