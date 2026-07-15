//@ expect val 60
with const i32 {
    A = 10
    B = 20
    C = 30
}
fn main() i32 { return A + B + C }
