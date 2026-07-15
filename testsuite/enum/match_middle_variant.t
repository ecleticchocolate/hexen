//@ expect val 42
enum Color { u32 Red  u32 Green  u32 Blue }
fn main() i32 {
    Color c = .Green{42}
    match c {
        .Red{v} { return -1 }
        .Green{v} { return (i32) v }
        .Blue{v} { return -2 }
    }
    return -3
}
