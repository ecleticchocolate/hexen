//@ expect stdout
//@ | x=9 y=3
extern fn printf(u8* f,...) i32
struct P { i32 x  i32 y }
fn main() i32 { P p={3,9}  unpack { .x=p.y, .y=p.x } = p  printf("x=%d y=%d\n", p.x, p.y)  return 0 }
