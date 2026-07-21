//@ expect err last field
fn main() i32 {
    match struct { i32; i32 } {
        struct { A...; B } { return 1 }
        else { return 0 }
    }
}
