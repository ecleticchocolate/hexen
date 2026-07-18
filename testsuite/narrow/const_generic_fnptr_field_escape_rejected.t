//@ expect err comptime-heap pointer/fn-symbol can't escape
// Regression: a struct-typed const-generic VALUE holding a function-pointer
// field used to segfault the compiler at runtime. materialize_agg_param
// (types.c) copied the field's raw arena bytes -- a Symbol* (the compiler's
// own process-space heap pointer, per ce_eval_ident's fn-symbol convention)
// -- verbatim into the emitted binary's data section, with no check that a
// function reference is exactly as meaningless at runtime as a comptime
// pointer already is. ConstEval_AggHasEscapingPtr (which parse_const_decl
// already used for ordinary `const Agg X = {...}`) now also flags
// TYPE_FUNCTION fields, and materialize_agg_param calls it before persisting
// -- so this fails cleanly at compile time instead of crashing the JIT.
struct VCfg { fn(i32) i32 op  i32 tag }
struct Wrapped[T, VCfg C] { T val }
impl Wrapped[T, C] {
    fn apply() i32 { return C.op(self.val) + C.tag }
}
fn triple(i32 x) i32 { return x * 3 }
fn main() i32 {
    Wrapped[i32, {.op = triple, .tag = 100}] w = {.val = 7}
    return w.apply()
}
