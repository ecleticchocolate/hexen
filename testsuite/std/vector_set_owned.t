//@ use std/option.t std/iterator.t std/vector.t
//@ expect val 0
// REGRESSION (was stdtests/vector_set_owned.t): set() on a Vector of owning
// elements must destroy the displaced element exactly once. It used to call
// .__delete() explicitly, which does NOT suppress the scope-exit destructor --
// so the old value was released twice.
fn main() i32 {
    Vector[Vector[u8]] v = Vector[Vector[u8]].create()
    Vector[u8] a = Vector[u8].create()
    a.push(1)
    Vector[u8] b = Vector[u8].create()
    b.push(2)
    v.push(a)
    if (!v.set(0, b)) { return 1 }
    return 0
}
