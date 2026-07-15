//@ expect val 49055057
extern fn snprintf(u8* buf, u64 size, u8* fmt, ...) i32
fn main() i32 {
    u8[64] buf
    snprintf(&buf[0], 64, "%d,%d,%d,%d,%d,%d,%d,%d,%d", 1,2,3,4,5,6,7,8,9)
    i32 c0 = (i32)buf[0]
    i32 c12 = (i32)buf[12]
    i32 c16 = (i32)buf[16]
    return c0 * 1000000 + c12 * 1000 + c16
}
