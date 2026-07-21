//@ expect val 1
// bare name `a` asserts field name; positional `u8` matches any second field name
struct S { i32 a  u8 b }
fn probe[T]() i32 {
    match T { struct { i32 a  u8 } { return 1 } else { return 2 } }
    return -1
}
fn main() i32 { return probe[S]() }
