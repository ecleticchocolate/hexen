//@ expect val 60
fn main() i32 { i32[3] arr={10,20,30}  i32[3]* ap=&arr  unpack { a, b, c } = ap  return a+b+c }
