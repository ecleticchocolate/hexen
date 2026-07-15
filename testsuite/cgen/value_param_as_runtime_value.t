//@ expect val 20
struct Scaler[T, u32 K] { T v }
impl Scaler[T, u32 K] {
    fn scaled() T { return self.v * K }
}
fn main() i32 {
    Scaler[i32, 5] s = {.v = 4}
    return s.scaled()   // 20
}
