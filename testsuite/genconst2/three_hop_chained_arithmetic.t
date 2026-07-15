//@ expect val 123
struct Vec[T, u32 N] { T[N] e }
struct Padded[T, u32 N] { Vec[T, N + 2] v }
struct DoublePadded[T, u32 N] { Padded[T, N + 3] p }
fn main() i32 {
    DoublePadded[i32, 5] d
    d.p.v.e[9] = 123
    return d.p.v.e[9]
}
