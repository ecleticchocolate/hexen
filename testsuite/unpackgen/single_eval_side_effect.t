//@ expect stdout
//@ | calls=1 a=4 b=8
extern fn printf(u8* f,...) i32
struct P { i32 x  i32 y }
i32 CALLS=0
fn mk() P { CALLS=CALLS+1  return {4,8} }
fn main() i32 { i32 a=0 i32 b=0  unpack { a, b } = mk()  printf("calls=%d a=%d b=%d\n", CALLS, a, b)  return 0 }
