//@ expect val 5
// Regression: a generic param can itself stand for an unapplied, still-generic
// template ("higher-kinded" argument) -- `HKT[M, T] { M[T] data }` binds M to
// a bare `Box` (no [args]) at the call site, and `M[T]` inside the body
// applies it. Requires two things that didn't exist before: (1) the parser
// letting a bare generic struct name ride through as a type argument instead
// of hard-erroring "generic struct used without type arguments", and (2)
// Type_Substitute recognizing, once M resolves to a still-generic template,
// that a FOLLOWING array-postfix `[T]` was never an array size -- it's the
// deferred application -- and routing it through Struct_Instantiate instead
// of ConstEval. sizeof proves it's real: a genuine array-of-anything here
// would NOT be 4 bytes.
struct Box[T] { T val }
struct HKT[M, T] { M[T] data }

fn main() i32 {
    HKT[Box, i32] h
    h.data = { .val = 5 }
    return h.data.val
}
