//@ expect val 60
// M[T][N] -- an ARRAY of an HKT instantiation, not a second HKT argument.
// The leftmost-outermost array-postfix fold (same rule u32[2][3] uses) makes
// the LEFTMOST bracket [T] the OUTERMOST TYPE_ARRAY node once parsed, but T
// is the template's type ARGUMENT (applies to M first) while N (innermost,
// next to bare M) is the genuine array count wrapping the result -- checking
// one array level at a time gets this backwards, treating N as if it were a
// second type argument to M. array_substitute_maybe_hkt (types.c) walks the
// whole bracket chain once, consumes exactly enough LEFTMOST levels to
// saturate the template's own declared arity, and rebuilds anything left
// over as a genuine array wrapping the instantiated result -- so this
// generalizes to any template arity, not just the 1-arg case here.
struct Box[T] { T val }
struct HKTN[M, T, u32 N] { M[T][N] items }

fn main() i32 {
    HKTN[Box, i32, 3] h
    h.items[0] = { .val = 10 }
    h.items[1] = { .val = 20 }
    h.items[2] = { .val = 30 }
    return h.items[0].val + h.items[1].val + h.items[2].val
}
