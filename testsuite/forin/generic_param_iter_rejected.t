//@ expect err for-in iterable must be
fn sumarr[T](T arr) i32 { i32 s = 0  for i32 x in arr { s = s + x }  return s }
fn main() i32 { i32[3] a = {1,2,3}  return sumarr[i32[3]](a) }
