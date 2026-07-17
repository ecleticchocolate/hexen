//@ expect val 16
struct Vec { i32[4] data }
impl Vec { fn __index(u32 i) i32* { return &self.data[i] } }
struct P { i32 x  i32 y }
fn main() i32 { Vec v  P p={7,9}  unpack { .x=v[0], .y=v[1] } = p  return v.data[0]+v.data[1] }
