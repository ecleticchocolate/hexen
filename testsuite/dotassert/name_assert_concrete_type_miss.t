//@ expect val 2
// `.name` must assert even when the field's TYPE slot is concrete (not a hole).
// Regression: substitution re-registered the anon pattern under a key built from
// field TYPES only, dropping the ` .name` suffix and name_asserted, so the
// pattern dedup'd onto the assertion-free `struct {i32}` twin and never asserted.
struct S { i32 a }
fn probe[T]() i32 {
    match T { struct { i32 zzz } { return 1 } else { return 2 } }
    return -1
}
fn main() i32 { return probe[S]() }
