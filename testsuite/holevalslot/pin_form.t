//@ expect val 30
// Explicit value-type in a WILDCARD-head pattern (the one compromise).
// A hole head M supplies no declaration, so bare `M[E, N]` cannot use N as a
// value in the arm body. Writing the value's type -- `M[E, u32 N]` -- states
// the kind the head can't, mirroring the declaration spelling `Vec[T, u32 N]`.
struct Vec[T, u32 N] { T[N] e }
fn f[S]() i32 { match S { struct M[E, u32 N] { return (i32)N } else { return -1 } } return -2 }
fn main() i32 { return f[Vec[i32, 30]]() }
