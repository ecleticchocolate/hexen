//@ expect val 70
struct Scaled[f64 K] { i32 dummy }
impl Scaled[f64 K] {
    fn scale(f64 x) f64 { return x * K }
}
fn main() i32 {
    Scaled[3.5] s = {.dummy = 0}
    f64 r = s.scale(2.0)
    return (i32)(r * 10.0)
}
