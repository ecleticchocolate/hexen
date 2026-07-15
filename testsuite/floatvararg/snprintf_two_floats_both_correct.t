//@ expect val 49050
extern fn snprintf(u8* buf, u64 size, u8* fmt, ...) i32
fn main() i32 {
    u8[32] buf
    snprintf(&buf[0], 32, "%.1f,%.1f", 1.5, 2.5)
    i32 c0 = (i32) buf[0]
    i32 c4 = (i32) buf[4]
    return c0 * 1000 + c4
}
