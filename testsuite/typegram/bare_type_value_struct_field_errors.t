//@ expect err a type cannot be used as a value
struct P { i32 x }
fn main() i32 { P p = {.x = void}  return p.x }
