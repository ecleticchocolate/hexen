//@ expect val 8
fn main() i32 {
    match struct { i32 x  i32 y } {
        struct { A a  B b } { return (i32)sizeof(A) + (i32)sizeof(B) }
        else { return 0 }
    }
}
