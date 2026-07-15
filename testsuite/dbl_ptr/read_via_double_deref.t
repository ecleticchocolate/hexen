//@ expect val 42
fn main() i32 {
    u32 x = 42
    u32* p = &x
    u32** pp = &p
    return (i32) **pp
}
