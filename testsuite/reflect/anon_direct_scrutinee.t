//@ expect val 8
fn main() i32 {
    match struct { i32; i32 } {
        struct { A; B } { return (i32)sizeof(A) + (i32)sizeof(B) }
        else { return 0 }
    }
}
