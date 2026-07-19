//@ expect val 30
// Struct payload through the unpack/generic-calls-generic path. A struct type param
// cannot be accidentally guessed as i32, so this proves the enclosing param is
// really substituted (concrete Pair), not defaulted.
struct Pair { i32 a  i32 b }
fn inner[U](U v) U { return v }
fn outer_diff[T](T v) T { unpack x = inner(v); return x }
fn main() i32 {
    Pair p = {.a = 10, .b = 20}
    Pair r = outer_diff(p)
    return r.a + r.b
}
