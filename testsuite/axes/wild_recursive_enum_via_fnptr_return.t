//@ expect val 42
// Wild: self-referential generic enum where the recursion is through a
// FUNCTION POINTER RETURN TYPE, not a struct pointer field. Does the
// recursive type `Lazy[T]` resolve at all when the cycle-breaker is a
// function signature instead of the usual `Node[T]* next`?
enum Lazy[T] { fn() Lazy[T] Thunk  T Done }
fn force(Lazy[i32] l) i32 {
    match l {
        .Done(v) { return v }
        .Thunk(f) { return force(f()) }
    }
}
fn final_thunk() Lazy[i32] { return .Done(42) }
fn middle_thunk() Lazy[i32] { return .Thunk(final_thunk) }
fn main() i32 {
    return force(.Thunk(middle_thunk))
}
