//@ expect val 4
fn main() i32 {
    u32 x = 0x01020304
    u8* bp = (u8*) &x
    // little-endian: first byte is 0x04
    return (i32) bp[0]
}
