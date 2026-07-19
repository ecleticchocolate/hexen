//@ expect val 42
// Same shape as the diff-name case but the callee reuses the name T. This also
// regressed on the eager parse-time path; kept as a distinct test so the two
// cannot silently diverge again (an earlier build passed one but not the other).
fn inner[T](T v) T { return v }
fn outer[T](T v) T { unpack x = inner(v); return x }
fn main() i32 { return outer(42) }
