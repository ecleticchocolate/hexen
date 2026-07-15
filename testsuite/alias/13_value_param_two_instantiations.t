//@ expect val 12
alias Buf[u32 N] = u8[N]
fn main() i32 { Buf[4] a  Buf[8] b  return (i32)(sizeof(a) + sizeof(b)) }
