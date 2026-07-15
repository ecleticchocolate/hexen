//@ expect val 99
struct Arr { i32[5] vals }
fn make() Arr {
    Arr a
    a.vals[0] = 99
    return a
}
const Arr A = make()
fn main() i32 { return A.vals[0] }
