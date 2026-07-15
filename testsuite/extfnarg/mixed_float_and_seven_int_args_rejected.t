//@ expect err mixes more than 6 integer
extern fn printf(u8* fmt, ...) i32
fn main() i32 {
    u32 a=1  u32 b=2  u32 c=3  u32 d=4  u32 e=5  u32 f=6
    f64 x = 9.5
    printf("%u %u %u %u %u %u %.1f\n", a, b, c, d, e, f, x)
    return 0
}
