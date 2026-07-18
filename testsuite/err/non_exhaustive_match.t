//@ expect err not exhaustive
enum Color { u32 Red  u32 Green  u32 Blue }
fn main() i32 {
    Color c = .Red(1)
    match c {
        .Red(v) { return (i32) v }
        .Green(v) { return (i32) v }
    }
    return 0
}
