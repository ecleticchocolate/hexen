//@ expect val 16
struct Vec { i32[4] d }
impl Vec { fn __index(u32 i) i32* { return &self.d[i] } }
alias VP = Vec*
struct P { i32 x  i32 y }
fn main() i32 { Vec v  P p={7,9}  VP vp=&v  unpack { .x=vp[0], .y=vp[1] } = p  return v.d[0]+v.d[1] }
