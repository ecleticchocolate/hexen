//@ expect val 4
alias Buf[u32 N] = u8[N]
fn main() i32 { Buf[4] b  b[0]=7  return (i32)sizeof(b) }
