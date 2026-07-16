//@ expect val 6
// Regression: a bare brace literal RHS (`w = {1, 2, 3}`) never reached
// __assign dispatch at all -- resolve_brace_literal ran first, unconditionally
// against the STRUCT's own shape (assuming plain positional-struct-literal
// construction), and errored on arity mismatch before try_rewrite_operator_
// method ever got a chance to check for __assign. Fixed by resolving a bare
// literal argument against __assign's PARAMETER type before the fit-check,
// the same way every other call-argument site already resolves a literal
// against its target parameter.
struct Wrapper { i32 v }
impl Wrapper {
    fn __assign(i32[3] arr) void {
        self.v = arr[0] + arr[1] + arr[2]
    }
}
fn main() i32 {
    Wrapper w = { .v = 0 }
    w = { 1, 2, 3 }
    return w.v
}
