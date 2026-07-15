//@ expect val 42
fn make() struct { i32 x  i32 y } {
    struct { i32 x  i32 y } r
    r.x = 40  r.y = 2
    return r
}
fn main() i32 { struct { i32 x  i32 y } v = make()  return (i32)(v.x + v.y) }
