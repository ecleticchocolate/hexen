//@ expect val 99
// A chain of unpack-bound generic calls inside one generic body: each unpack decl
// re-triggers the eager parse-time inference, and every link must bind through
// the enclosing T. Guards against the fix only working for a single level.
fn id[U](U v) U { return v }
fn chain[T](T v) T { unpack a = id(v); unpack b = id(a); unpack c = id(b); return c }
fn main() i32 { return chain(99) }
