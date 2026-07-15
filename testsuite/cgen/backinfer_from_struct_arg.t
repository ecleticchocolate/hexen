//@ expect val 42
struct Vec[T, u32 N] { T[N] e }
fn firstEl[T, u32 N](Vec[T, N] v) T { return v.e[0] }
fn main() i32 {
    Vec[i32, 4] v
    v.e[0] = 42
    return firstEl(v)
}
