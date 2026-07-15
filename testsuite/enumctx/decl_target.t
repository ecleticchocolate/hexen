//@ expect val 42
enum Ev { u32 A  u32 B  None }
fn main() i32 {
    Ev e = .B{42}
    match e { .A{v} { return -1 }  .B{v} { return (i32) v }  .None { return -2 } }
    return -3
}
