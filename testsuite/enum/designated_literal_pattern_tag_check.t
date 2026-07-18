//@ expect val 99
// Regression: {.Variant = v} match patterns must emit a tag-equality check,
// same as .Variant(v) does. Before the fix, this designated-literal pattern
// shape fell through to the plain struct-destructure branch, which reads
// scrut.Some unconditionally with NO tag check -- silently returning
// whatever garbage bytes sit at that field's offset even when the real tag
// is .None, instead of failing to match and falling to the .None arm.
enum Option[T] { T Some  None }
fn main() i32 {
    Option[u32] r = .None
    match r {
        {.Some = v} { return (i32) v }
        .None       { return 99 }
    }
    return -1
}
