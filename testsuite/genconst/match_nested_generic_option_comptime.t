//@ expect val 50
// Flagship: a `match` whose scrutinee is a method call returning a generic param
// that resolves to a NESTED generic enum (Box[Option[i32]].get() -> Option[i32]),
// evaluated at COMPTIME inside a const block. Exercises const-generic-adjacent
// generic impls + nested Option + value-match-on-a-generic-call-result all through
// the comptime evaluator. Regressed during the match rewrite (the comptime lowering
// path skipped infer_generic on the scrutinee); this pins it.
enum Option[T] { T Some  None }
struct Box[T] { T item }
impl Box[T] { fn get() T { return self.item } }
fn build() i32 {
    Box[Option[i32]] b = { .item = .Some(50) }
    i32 r = 0
    match b.get() {
        .Some(v) { r = v }
        .None {}
    }
    return r
}
fn main() i32 {
    i32 res = 0
    const { res = build() }
    return res
}
