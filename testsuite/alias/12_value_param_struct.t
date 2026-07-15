//@ expect val 4
alias Fixed[u32 N] = struct { u8[N] data  u32 len }
fn main() i32 { Fixed[4] f  f.len=4  f.data[0]=9  return (i32)sizeof(f.data) }
