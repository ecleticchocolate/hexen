//@ expect val 4
struct S { i32 x  i32 y }
fn main() i32 {
    match S {
        struct { A a  A b } { return (i32)sizeof(A) }
        else { return 99 }
    }
}
