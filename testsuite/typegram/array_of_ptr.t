//@ expect val 5
fn main() i32 {
    u32 v = 5
    u32*[2] pa
    pa[0] = &v
    return (i32)(*pa[0])
}
