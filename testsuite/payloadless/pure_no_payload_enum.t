//@ expect val 2
enum Direction { North South East West }
fn main() i32 {
    Direction d = .East
    i32 r = 0
    match d {
        .North { r = 0 }
        .South { r = 1 }
        .East { r = 2 }
        .West { r = 3 }
    }
    return r
}
