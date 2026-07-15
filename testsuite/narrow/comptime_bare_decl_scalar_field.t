//@ expect val 99
struct S { i32 x }
fn make() S {
    S a
    a.x = 99
    return a
}
const S A = make()
fn main() i32 { return A.x }
