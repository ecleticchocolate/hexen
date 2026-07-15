//@ expect val 7
// A fn-typed const-generic parameter (see 02_const_generic_vtable_zero_cost.t)
// called from inside a `const`-evaluated function, not just at runtime.
//
// Bug this pins: calling through F required folding an AST_CAST wrapping the
// param's bound value down to a real function Symbol*. ce_eval_cast's
// "does this cast legitimately name a function" guard only recognized one
// shape (a bare AST_IDENT naming a SYM_FUNCTION) -- but a fn-typed
// const-generic parameter is monomorphized by clone_ast into the OTHER
// legitimate shape, an AST_INT_LITERAL tagged LIT_FN_SYMBOL carrying the
// same Symbol* as raw bits. The runtime/JIT path built and called this
// exact construction correctly; asking ConstEval to fold it a second time,
// one level up inside a `const`, hit the narrower guard and failed with
// "not a constant expression" -- the runtime and comptime paths had quietly
// diverged on the identical program. Fixed by recognizing both shapes.
struct DynA[fn(void*) i32 F] { void* obj }
impl DynA[F] { fn area() i32 { return F(self.obj) } }
fn plain_fn(void* p) i32 { return 7 }
fn compute() i32 {
    DynA[plain_fn] d = { .obj = null }
    return d.area()
}
const i32 RESULT = compute()
fn main() i32 { return RESULT }
