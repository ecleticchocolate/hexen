//@ expect err not exhaustive
fn main() i32 {
    u32 x = 5
    match x {
        5 { return 1 }
    }
    return 0
}
