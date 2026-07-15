//@ expect val 77
fn main() i32 {
    i32 x = 7
    match x {
        1 { return 1 }
        7 { return 77 }
        else { return -1 }
    }
    return 0
}
