//@ expect val 8
struct Point { i32 x  i32 y }
fn main() i32 {
    match Point {
        struct { A x  B y } { return (i32)sizeof(A) + (i32)sizeof(B) }
        else { return 99 }
    }
}
