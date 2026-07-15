//@ expect val 42
fn main() i32 {
    struct { struct { struct { i32 v } inner } mid } outer
    outer.mid.inner.v = 42
    return (i32)outer.mid.inner.v
}
