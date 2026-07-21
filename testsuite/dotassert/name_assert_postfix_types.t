//@ expect val 12
// `.name` on fields whose types use postfix/fn grammar rather than a bare name.
struct S { i32* a }
struct W { i32* zz }
fn probe[T]() i32 {
    match T { struct { E* a } { return 1 } else { return 2 } }
    return -1
}
fn main() i32 { return probe[S]()*10 + probe[W]() }
