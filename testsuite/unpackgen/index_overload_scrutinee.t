//@ expect val 56
struct P { i32 x  i32 y }
struct Grid { P[4] cells }
impl Grid { fn __index(u32 i) P* { return &self.cells[i] } }
fn main() i32 { Grid g  g.cells[2]={5,6}  unpack { .x=a, .y=b } = g[2]  return a*10+b }
