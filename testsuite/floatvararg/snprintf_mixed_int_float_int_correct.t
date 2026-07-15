//@ expect val 49050051
extern fn snprintf(u8* buf, u64 size, u8* fmt, ...) i32
fn main() i32 {
    u8[32] buf
    snprintf(&buf[0], 32, "%d,%.1f,%d", 1, 2.5, 3)
    i32 c0 = (i32) buf[0]
    i32 c2 = (i32) buf[2]
    i32 c6 = (i32) buf[6]
    return c0 * 1000000 + c2 * 1000 + c6
}
