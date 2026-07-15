//@ expect val 77
fn main() i32 {
    u32 x = 1
    u32* p = &x
    u32** pp = &p
    **pp = 77
    return (i32) x
}
