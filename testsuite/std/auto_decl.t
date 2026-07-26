//@ use std/option.t std/iterator.t std/vector.t std/array.t std/hashmap.t
//@ expect val 42
fn main() i32 {
    auto m = HashMap[i32, i32].create()
    m[1] = 42
    return m[1]
}
