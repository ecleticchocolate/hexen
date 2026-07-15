//@ expect val 7
enum Color { u32 Red  u32 Green  u32 Blue }
fn main() i32 {
    Color c = .Red{7}
    match c {
        .Red{v} { return (i32) v }
        .Green{v} { return -1 }
        .Blue{v} { return -2 }
    }
    return -3
}
