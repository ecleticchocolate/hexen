//@ expect val 7
enum Ev { u32 A  u32 B  None }
fn make() Ev { return .A{7} }
fn main() i32 {
    match make() { .A{v} { return (i32) v }  .B{v} { return -1 }  .None { return -2 } }
    return -3
}
