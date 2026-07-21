//@ expect val 2
// name-only assertion: type slot is a hole, name still asserted
struct S { i32 a }
fn probe[T]() i32 {
    match T { struct { N zzz } { return 1 } else { return 2 } }
    return -1
}
fn main() i32 { return probe[S]() }
