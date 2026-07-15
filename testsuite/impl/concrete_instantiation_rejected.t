//@ expect err specialization
// `impl Vector[u8]` READS as "a method only on Vector[u8]", but the call site mangles
// with the generic BASE name, so the method silently answered calls on Vector[u32] too.
// Specialization is a real design decision; an ignored bracket must not grant it by
// accident. §1: loud failure over silent wrongness.
struct Box[T] { T v }
impl Box[u8] { fn tag() u32 { return 8 } }
fn main() u32 { return 0 }
