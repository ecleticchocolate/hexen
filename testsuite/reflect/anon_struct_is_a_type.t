//@ expect val 42
fn main() i32 {
    struct { i32 x  i32 y } p
    p.x = 10  p.y = 32
    return (i32)(p.x + p.y)
}
