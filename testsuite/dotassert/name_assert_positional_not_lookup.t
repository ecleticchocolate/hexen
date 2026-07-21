//@ expect val 2
// assertion is positional: `.b` in slot 0 must match field 0's name, not search
struct S { i32 a  i32 b }
fn probe[T]() i32 {
    match T { struct { i32 b  i32 a } { return 1 } else { return 2 } }
    return -1
}
fn main() i32 { return probe[S]() }
