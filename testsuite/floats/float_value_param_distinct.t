//@ expect stdout
//@ | K=2.5 -> 10.000000
//@ | K=3.5 -> 14.000000
//@ | K=2.5 -> 25.000000
extern fn printf(u8* fmt, ...) i32
struct Scaled[T, f64 K] { T v }
impl Scaled[T, K] {
    fn apply() f64 { return (f64)self.v * K }
}
fn main() i32 {
    Scaled[i32, 2.5] a = {.v = 4}
    Scaled[i32, 3.5] b = {.v = 4}
    Scaled[i32, 2.5] c = {.v = 10}
    printf("K=2.5 -> %f\n", a.apply())
    printf("K=3.5 -> %f\n", b.apply())
    printf("K=2.5 -> %f\n", c.apply())
    return 0
}
