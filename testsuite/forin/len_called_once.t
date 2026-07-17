//@ expect stdout
//@ | lc=1 s=6
extern fn printf(u8* f,...) i32
struct Vec { i32[3] d  i32 lencalls }
i32 LC=0
impl Vec { fn __index(u32 i) i32* { return &self.d[i] }  fn len() u64 { LC=LC+1  return 3 } }
fn main() i32 { Vec v  v.d={1,2,3}  i32 s=0  for i32 x in v { s=s+x }  printf("lc=%d s=%d\n", LC, s)  return 0 }
