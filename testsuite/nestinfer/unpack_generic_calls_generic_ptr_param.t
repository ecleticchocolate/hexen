//@ expect val 33
// Inner generic takes U* and the outer passes &v (type T*). The enclosing-param
// bind must reach through the pointer: T* vs U* unifies structurally down to the
// bare T vs U, where the exception fires. Covers the compound-then-bare path.
fn deref_id[U](U* p) U { return *p }
fn outer[T](T v) T { unpack x = deref_id(&v); return x }
fn main() i32 { i32 n = 33  return outer(n) }
