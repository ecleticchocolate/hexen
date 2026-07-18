//@ expect val 15
// Regression: a generic function taking a variadic pack (`V... fns`) called
// with EXPLICIT type args that already fully match the type-param count
// (e.g. `pack_it[SomeStruct](x)`) never got its pack argument expanded when
// folded inside `const {}`. pack_expand_call_args (types.c) -- which bundles
// trailing call arguments into one synthesized anonymous-struct value -- only
// ever ran as infer_generic's own first action; ce_eval_call (constexpr.c)
// only called infer_generic when the type args WEREN'T already fully known,
// so a call whose explicit type args already satisfied the full param count
// skipped it entirely, leaving node->call.args un-bundled and mismatched
// against the callee's real (post-expansion) arity. Found via tensor2.t's
// "structural VTable generation" showcase (a real, pre-existing composition
// of generics + packs + fn-pointers), which failed to fold under const{}
// until this was fixed. ce_eval_call now calls infer_generic unconditionally
// -- its OWN early-return (right after the pack-expansion step) already
// makes this a no-op for the type-arg-inference part once args are explicit,
// so this only ever ADDS the missing pack expansion.
alias Getter = struct { (fn(void*) i32) get }
fn t_get[T](void* p) i32 {
    T* o = (T*)p
    return o.get()
}
fn dyn_obj[T, V](T* o, V... fns) struct { void* obj  V vt } {
    return { .obj = (void*)o, .vt = fns }
}
struct HasGet { i32 val }
impl HasGet { fn get() i32 { return self.val } }
fn test() i32 {
    HasGet hg = {.val = 15}
    struct { void* obj  Getter vt } d = dyn_obj[HasGet, Getter](&hg, t_get[HasGet])
    return d.vt.get(d.obj)
}
fn main() i32 {
    i32 res = 0
    const { res = test() }
    return res
}
