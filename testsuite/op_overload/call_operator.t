//@ expect val 15
struct Adder { i32 n }
impl Adder {
    fn __call(i32 x) i32 { return self.n + x }
}
fn main() i32 {
    Adder a = { .n = 5 }
    return a(10)
}
