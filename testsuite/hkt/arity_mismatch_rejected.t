//@ expect err expects 2 type arguments
// A template whose declared arity needs MORE type-shaped brackets than are
// actually available before hitting a genuine array-size value must be a
// clean compile error, not silent wrongness. Pair2 (arity 2) applied via
// M[T][N] where N is a real u32 const-generic value (not a type) used to
// return NULL from array_substitute_maybe_hkt's arg-resolution loop and fall
// through to the ordinary array-size fold -- leaving M permanently unapplied
// (still is_generic), which Type_SizeOf's TYPE_STRUCT case silently read as
// 0 bytes instead of erroring. Confirmed via sizeof before the fix (real
// miscompile, not hypothetical); now a clean, immediate diagnostic instead.
struct Pair2[A, B] { A first  B second }
struct HKT3[M, T, u32 N] { M[T][N] data }
fn main() i32 {
    HKT3[Pair2, i32, 5] h
    return (i32)sizeof(h)
}
