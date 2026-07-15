//@ expect val 30
// The kind-driven part: `pick[u32, 3]` mixes a TYPE slot and a VALUE slot, and the
// callee is declared below. Classifying slot 2 as a value (not a type) is only
// possible with the callee's param_kinds, which pass 0b supplies.
fn caller() i32 { return (i32)pick[u32, 3]() }
fn pick[T, u32 N]() u32 { return N * 10 }
fn main() i32 { return caller() }
