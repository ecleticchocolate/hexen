//@ expect val 12
// The packaged `super A base` field aliases the promoted prefix, so it costs NO
// extra storage: sizeof(B) is sizeof(A) + own fields, not sizeof(A) counted twice.
// Under the old duplicate-storage semantics this was 20 (8 + 8 + 4); aliasing = 12.
struct A { u32 x  u32 z }         // 8
struct B { super A base  u32 y }  // 8 (shared A prefix) + 4 (y)
fn main() i32 { return (i32)sizeof(B) }
