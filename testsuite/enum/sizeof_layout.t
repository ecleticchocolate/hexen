//@ expect val 8
enum Ev { u32 A  bool B }
fn main() i32 {
    // tag(4) + max(sizeof u32=4, sizeof bool=1) = 8
    return (i32) sizeof(Ev)
}
