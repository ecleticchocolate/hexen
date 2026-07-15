//@ expect val 3
alias Vec[T, u32 N] = T[N]
fn main() i32 { Vec[i32, 3] v  v[0]=5  v[2]=9  return (i32)(sizeof(v)/sizeof(i32)) }
