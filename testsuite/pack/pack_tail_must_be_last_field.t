//@ expect err last field
fn main() i32 {
    match struct { i32 x  i32 y } {
        struct { A... a  B b } { return 1 }
        else { return 0 }
    }
}
