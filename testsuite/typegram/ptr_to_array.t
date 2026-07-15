//@ expect val 8
fn main() i32 {
    u32[4] a
    a[3] = 8
    u32[4]* p = &a
    return (i32)((*p)[3])
}
