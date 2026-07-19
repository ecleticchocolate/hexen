//@ expect val 1
// A doubly-nested generic call `id(id(5))` as an operand of an operator (here `==`).
// The outer id must infer its T from the inner id's result; that requires resolving
// the inner call first. infer_generic's bottom-up arg pass now runs infer_generic on
// a generic-call argument before reading its type -- so this works in EVERY operator
// context (+, *, &, unary, comparison), not just where a top-down target existed
// (decl init / return / call arg already worked). Single-place fix, not per-operator.
fn id[T](T x) T { return x }
fn main() i32 {
    if id(id(5)) == 5 { return 1 }
    return 0
}
