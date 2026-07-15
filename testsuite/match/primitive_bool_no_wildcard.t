//@ expect val 1
fn main() i32 {
    bool b = true
    match b {
        true { return 1 }
        false { return 0 }
    }
    return 5
}
