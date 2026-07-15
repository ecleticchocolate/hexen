//@ expect val 57
extern fn snprintf(u8* buf, u64 size, u8* fmt, ...) i32
fn main() i32 {
    u8[32] buf
    snprintf(&buf[0], 32, "%d,%d,%.1f", 1, 2, 9.5)
    return (i32)buf[4]
}
