//@ expect val 0
// Same regression class, for __call. Before the fix, this errored outright
// ("not a constant expression") rather than silently computing a wrong
// answer, since ce_eval_call had no fallback interpretation for a struct
// value in call position at all.
struct Adder { i32 n }
impl Adder {
    fn __call(i32 x) i32 { return 777 }
}
fn build() i32 {
    Adder a = { .n = 5 }
    return a(10)
}
const i32 R = build()
fn main() i32 { if (R != 777) { return 1 } return 0 }
