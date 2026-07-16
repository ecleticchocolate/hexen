//@ expect val 0
// Same regression class as dispatch_actually_used.t: __call deliberately
// ignores its argument and self, so the test fails loudly if dispatch is
// ever wrong (there is no lanewise fallback for AST_CALL to coincidentally
// agree with, but this locks in the actual dispatched value regardless).
struct Adder { i32 n }
impl Adder {
    fn __call(i32 x) i32 { return 777 }
}
fn main() i32 {
    Adder a = { .n = 5 }
    if (a(10) != 777) { return 1 }
    return 0
}
