//@ expect val 55
// Same root fix as 06 (ce_eval_ident's generic-fn-as-value fold), reached
// through a different shape: a generic function reference returned out of an
// ordinary function (`return id[i32]`), then stored in a local and called --
// all inside a `const` initializer. Confirmed to fail before the ce_eval_ident
// fix (same "not a constant expression" error) and pass after it, with no
// separate change needed -- both shapes bottom out in the identical
// AST_IDENT node (a generic symbol referenced with explicit type args).
fn id[T](T x) T { return x }
fn get_fp() fn(i32) i32 { return id[i32] }
fn one() i32 {
    fn(i32) i32 fp = get_fp()
    return fp(55)
}
const i32 ONE = one()
fn main() i32 { return ONE }
