//@ expect val 44
// A named field whose type is a generic instantiation with BOTH a type arg and a
// const-generic value arg: the `.a` assertion must not disturb either binding.
struct Box[T, u32 N] { T[N] e }
struct S { Box[i32,4] a }
fn probe[T]() i32 {
    match T {
        struct { Box[E,N] a } { return (i32)N * 10 + (i32)sizeof(E) }
        else { return 2 }
    }
    return -1
}
fn main() i32 { return probe[S]() }
