//@ use std/option.t std/iterator.t std/vector.t std/array.t std/hashmap.t
//@ expect val 42
// REGRESSION: `v[i]` folded the ADDRESS instead of the element at comptime.
//
// `__index` returns T*, so `v[i]` desugars to `*(v.__index(i))`. The deref was
// applied on the runtime path (Type_Infer / infer_generic) but not in ConstEval,
// so a const initializer baked in a byte offset -- 5 came back as 84, 10 as 160,
// always the value x16. Silently: len() and get() folded correctly the whole
// time, so nothing looked wrong.
fn vec_idx() i32 {
    Vector[i32] v = Vector[i32].create()
    v.push(3)  v.push(5)
    return v[1]
}
fn map_idx() i32 {
    HashMap[i32, i32] m = HashMap[i32, i32].create()
    m[1] = 10
    m[2] = 20
    return m[1] + m[2]
}
fn arr_idx() i32 {
    Array[i32, 3] a
    a.fill(7)
    return a[2]
}
const i32 V = vec_idx()
const i32 M = map_idx()
const i32 A = arr_idx()
fn main() i32 { return V + M + A }
