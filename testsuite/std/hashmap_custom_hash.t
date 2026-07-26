//@ use std/option.t std/iterator.t std/vector.t std/array.t std/hashmap.t
//@ expect val 42
// A user __hash() wins over the structural fallback (the Hashable arm).
struct Cust { i32 id  i32 ignored }
impl Cust { fn __hash() u32 { return (u32)self.id * 2654435761 } }
fn main() i32 {
    HashMap[Cust, i32] m = HashMap[Cust, i32].create()
    Cust k = {.id = 7, .ignored = 1}
    m[k] = 42
    return m[k]
}
