//@ expect val 1
// Regression: operator-overload dispatch (__add, __eq, ...) never ran inside
// ConstEval at all -- it's a second, independent interpreter that walks the
// raw AST directly and never goes through Type_Infer's lazy AST_ADD ->
// AST_CALL rewrite. `a + b` / `a == b` on a struct with __add/__eq silently
// folded to the WRONG answer at compile time (built-in lanewise arithmetic /
// byte comparison) instead of calling the real method. __eq here does a
// genuinely different comparison (cross-multiplied fraction equality) than
// byte-equality would give for these two field values, so this fails loudly
// if dispatch regresses to the old lanewise/byte-compare fallback.
struct Frac { i32 num  i32 den }
impl Frac {
    fn __eq(Frac other) bool {
        return self.num * other.den == other.num * self.den
    }
}
fn check() bool {
    Frac a = { .num = 1, .den = 2 }
    Frac b = { .num = 2, .den = 4 }
    return a == b
}
const bool SAME = check()
fn main() i32 { if (SAME) { return 1 } return 0 }
