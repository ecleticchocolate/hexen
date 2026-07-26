//@ use std/option.t std/iterator.t std/vector.t std/array.t std/hashmap.t
//@ expect val 3
fn main() i32 {
    Array[i32, 3] a = {1, 2, 3}
    return a[2]
}
