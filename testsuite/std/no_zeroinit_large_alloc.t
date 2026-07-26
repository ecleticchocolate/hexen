//@ use std/option.t std/iterator.t std/vector.t
//@ expect val 1
// A large allocation must not be memset. `new[N] T` costs one malloc, not
// malloc + an N-byte zero fill -- that overhead scales with SIZE rather than
// with use, and no systems language pays it. This allocates 64 MB and only
// touches two bytes; with zero-init it would write all 64 MB first.
fn main() i32 {
    u8* big = new[67108864] u8
    big[0] = 1
    big[67108863] = 1
    i32 r = (i32)big[0]
    delete[] big
    return r
}
