//@ expect val 50
// The exact "does the zero-cost const-generic vtable (02) work inside a
// `const`?" question, with the harder variant: the dispatch function is
// itself a GENERIC function referenced with explicit type args
// (`t_area[Circle]`), not a plain function -- the shape 04 doesn't cover.
//
// Two independent bugs, both real, both found by tracing why this failed
// with "not a constant expression" instead of assuming it was a deliberate
// restriction:
//
// 1. ce_eval_cast's "does this cast legitimately name a function" guard
//    only recognized a bare AST_IDENT naming a SYM_FUNCTION. clone_ast
//    builds the OTHER legitimate shape for a fn-typed const-generic param --
//    an AST_INT_LITERAL tagged LIT_FN_SYMBOL carrying the same Symbol* as
//    raw bits -- which the guard didn't know about. (See 04.)
//
// 2. `t_area[Circle]` used as a const-generic ARGUMENT (not called, no
//    parens) still names the raw GENERIC TEMPLATE symbol at parse time --
//    it isn't instantiated until Typecheck_Tree normally would, later.
//    Meanwhile expr_mentions_generic_param's fail-safe (any TYPE_STRUCT/
//    TYPE_FUNCTION type-arg answers "might reference an outer param," see
//    its own comment) deferred the value, expecting some later
//    Type_Substitute call to resolve it against an enclosing generic frame.
//    But `compute()` here isn't generic at all -- there IS no outer frame,
//    so nothing was ever going to revisit the deferral, and it silently
//    read back as a scalar 0 the moment clone_ast monomorphized area()'s
//    body (which reads F). Fixed by eagerly instantiating a generic-symbol-
//    with-explicit-type-args right where a const-generic argument is
//    parsed, instead of waiting for typecheck to do it later.
struct Circle { i32 r }
impl Circle { fn area() i32 { return 42 } }
fn t_area[T](void* p) i32 { T* o = (T*)p  return o.area() }
struct DynA[fn(void*) i32 F] { void* obj }
impl DynA[F] { fn area() i32 { return F(self.obj) } }
fn compute() i32 {
    Circle c = { .r = 1 }
    DynA[t_area[Circle]] d = { .obj = (void*)&c }
    // The dispatch decision is fully resolved at compile time into the
    // TYPE itself -- sizeof proves nothing runtime-shaped (a vtable
    // pointer, a tag) is actually stored in the object.
    return d.area() + (i32)sizeof(d)
}
const i32 RESULT = compute()
fn main() i32 { return RESULT }
