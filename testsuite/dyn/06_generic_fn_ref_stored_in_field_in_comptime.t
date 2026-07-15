//@ expect val 99
// A GENERIC function referenced with explicit type args as a VALUE (not
// called directly -- stored into a fn-ptr-typed struct field) inside a
// `const` initializer. Previously failed with "not a constant expression":
// ce_eval_ident's fold for "a bare function name used as a value" explicitly
// excluded any symbol with generic_decl set ("Non-generic only for now" --
// its own comment), and nothing else in ce_eval_ident recognized an
// explicit-type-args generic ident either. The identical shape with a plain,
// non-generic function already folded correctly. Fixed by instantiating the
// generic symbol (Generic_Instantiate) right there, the same way an ordinary
// generic CALL already does, then folding the resulting concrete Symbol*
// exactly like the pre-existing non-generic branch does.
fn id_i32[T](T x) T { return x }
struct Holder { fn(i32) i32 fp }
fn one() i32 {
    Holder h = { .fp = id_i32[i32] }
    return h.fp(99)
}
const i32 ONE = one()
fn main() i32 { return ONE }
