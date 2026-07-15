//@ expect err Expected a type argument
struct Scaler[T, N] { T v }
impl Scaler[T, N] {
    fn scaled() T { return self.v * N }
}
fn main() i32 {
    Scaler[i32, 5] s = {.v = 4}
    return s.scaled()
}
