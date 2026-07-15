//@ expect val 82
struct Stack[T, u32 CAP] { T[CAP] data u32 len }
fn main() i32 {
    match Stack[i16, 8] {
        Stack[E, N] { return (i32)N * 10 + (i32)sizeof(E) }
        else { return 99 }
    }
}
