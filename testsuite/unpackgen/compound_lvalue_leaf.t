//@ expect val 33
fn main() i32 { i32 x=0  i32 y=0  i32*[2] ptrs={&x,&y}  struct{i32 i32} src={11,22}  unpack { *ptrs[0], *ptrs[1] } = src  return x+y }
