//@ expect val 100
struct Scaled[f32 K] { i32 dummy }
impl Scaled[f32 K] {
    fn scale(f32 x) f32 { return x * K }
}
fn main() i32 {
    Scaled[2.5] s = {.dummy = 0}
    f32 r = s.scale(4.0)
    return (i32)(r * 10.0)
}
