//@ use std/option.t std/iterator.t std/vector.t std/array.t std/hashmap.t
//@ expect val 9
// REGRESSION: __index on a generic struct must be able to call a sibling
// method. Typecheck_Tree's AST_INDEX case recursed into base/index BEFORE
// rewriting v[i] to v.__index(i), so the rewritten call never got AST_CALL's
// method-resolution and self.helper() failed with "has no field 'helper'".
// HashMap hit this via __index -> maybe_grow.
struct M[K, V] { K k  V v }
impl M[K, V] {
    fn helper() V { return self.v }
    fn __index(K key) V { return self.helper() }
}
fn main() i32 {
    M[i32, i32] m
    m.v = 9
    return m[1]
}
