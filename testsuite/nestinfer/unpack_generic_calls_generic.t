//@ expect val 55
// The eager parse-time typecheck of an `unpack` initializer (parse_unpack) is what
// makes generic-calls-generic inference run while the enclosing T is still abstract.
// This is the plain single-level case; the fix is pinned to the real construct.
fn inner[U](U v) U { return v }
fn outer[T](T v) T { unpack x = inner(v); return x }
fn main() i32 { return outer(55) }
