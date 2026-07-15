//@ expect err not exhaustive
fn main() i32 {
    u8* s = "hi"
    match s {
        5 { return 1 }
    }
    return 0
}
