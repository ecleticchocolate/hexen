//@ expect val 55
extern fn snprintf(u8* buf, u64 size, u8* fmt, ...) i32
fn main() i32 {
    u8[64] buf
    snprintf(&buf[0], 64, "%d,%d,%d,%d,%d,%d,%d", 1,2,3,4,5,6,7)
    return (i32)buf[12]
}
