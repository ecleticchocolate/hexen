//@ expect err cannot assign u32 to u32*
fn main() i32 { u32 x = 5; u32* p = x; return 0 }
