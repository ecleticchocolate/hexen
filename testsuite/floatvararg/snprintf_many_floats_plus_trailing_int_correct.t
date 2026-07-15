//@ expect val 49050051
extern fn snprintf(u8* buf, u64 size, u8* fmt, ...) i32
fn main() i32 {
    u8[32] buf
    snprintf(&buf[0], 32, "%.1f,%.1f,%.1f,%d", 1.5, 2.5, 3.5, 99)
    i32 c0 = (i32) buf[0]
    i32 c4 = (i32) buf[4]
    i32 c8 = (i32) buf[8]
    return c0 * 1000000 + c4 * 1000 + c8
}
