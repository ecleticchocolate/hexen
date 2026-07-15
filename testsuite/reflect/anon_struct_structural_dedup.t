//@ expect val 5
fn main() i32 {
    struct { i32 x } a
    struct { i32 x } b
    a.x = 5  b = a
    return (i32)b.x
}
