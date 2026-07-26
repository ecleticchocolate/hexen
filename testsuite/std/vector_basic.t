//@ use std/option.t std/iterator.t std/vector.t
//@ expect val 6
fn main() i32 {
    Vector[i32] v = Vector[i32].create()
    v.push(1)  v.push(2)  v.push(3)
    i32 sum = 0
    for u32 i = 0 to v.len() { i32 e = v[i]  sum = sum + e }
    return sum
}
