//@ expect val 4
struct Scaler[u8 N] { u8 x }
impl Scaler[u8 N] {
    fn add_n(u8 v) u8 { return v + N }
}
fn main() i32 {
    Scaler[250] s = {.x = 0}
    return (i32)s.add_n(10)
}
