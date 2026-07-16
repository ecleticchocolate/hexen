//@ expect val 42
// `b.method[TypeArgs](args)` -- explicit generic type arguments on a METHOD
// call, not just a bare function name. Previously unparseable: the postfix
// loop treated `[` after `.field` as unconditional indexing, and the
// existing explicit-generic-call parser only fires for a bare identifier
// resolvable to a Symbol at parse time (impl methods aren't resolved until
// typecheck). Fixed via speculative parse: `[...]` right after `.name` is
// tried as a type-argument list only when immediately followed by `(`;
// anything else rewinds to ordinary indexing.
struct Box { i32 v }
impl Box {
    fn identity[T](T x) T { return x }
}
fn main() i32 {
    Box b = { .v = 0 }
    return b.identity[i32](42)
}
