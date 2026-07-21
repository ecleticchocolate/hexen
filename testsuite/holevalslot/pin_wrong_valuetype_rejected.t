//@ expect val 0
// The explicit pin value-type is CHECKED, not trusted: `M[E, u64 N]` must NOT
// match a slot whose value-type is u32 -- the value-type is part of the type's
// identity (Vec[..u32 N..] != Wec[..u64 N..]). Soundness guard.
struct Vec[T, u32 N] { T[N] e }
fn f[S]() i32 { match S { struct M[E, u64 N] { return 1 } else { return 0 } } return -1 }
fn main() i32 { return f[Vec[i32, 30]]() }
