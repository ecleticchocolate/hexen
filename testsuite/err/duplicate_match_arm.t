//@ expect err unreachable match arm
enum Dir { u32 North  u32 South }
fn main() i32 {
    Dir d = .North{1}
    match d {
        .North{v} { return (i32) v }
        .North{v} { return 0 }
        .South{v} { return (i32) v }
    }
    return -1
}
