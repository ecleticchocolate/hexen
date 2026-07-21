//@ expect val 1
struct S { i32 a }
fn probe[T]() i32 {
    match T { struct { i32 a } { return 1 } else { return 2 } }
    return -1
}
fn main() i32 { return probe[S]() }
