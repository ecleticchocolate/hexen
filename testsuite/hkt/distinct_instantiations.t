//@ expect val 1010
// Same HKT template (M), different T -- must produce genuinely distinct
// instantiations (Box[i32] vs Box[u8]), not a shared/aliased one. The
// sizeof difference (4 - 1 = 3) proves the two h.data fields are really
// different concrete types, not both silently i32 or both silently u8.
struct Box[T] { T val }
struct HKT[M, T] { M[T] data }

fn main() i32 {
    HKT[Box, i32] a
    HKT[Box, u8] b
    a.data = { .val = 1000 }
    b.data = { .val = 7 }
    return a.data.val + (i32)b.data.val + (i32)sizeof(a) - (i32)sizeof(b)
}
