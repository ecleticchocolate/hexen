//@ expect val 30
// A bound HKT head applied to a CONST-GENERIC VALUE argument.
// `M[E][N]` (stacked-bracket HKT spelling) where M is bound to `Vec[T, u32 N]`:
// the second bracket is a VALUE slot, so it is folded against the template's own
// declared param kind rather than required to be type-shaped. Previously only
// TYPE arguments could be applied to a bound head, so any template with a value
// param was unreachable through an HKT slot.
struct Vec[T, u32 N] { T[N] e }
struct HKT[M] { i32 x }
fn probe[T]() i32 {
    match T {
        HKT[M] { match Vec[i32, 30] { M[E][N] { return (i32)N } else { return 2 } } }
        else { return 3 }
    }
    return -1
}
fn main() i32 { return probe[HKT[Vec]]() }
