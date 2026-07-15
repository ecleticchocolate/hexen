//@ expect val 42
fn main() i32 {
    u32 x = 42
    u32* a = &x
    u32** b = &a
    u32*** c = &b
    return (i32) ***c
}
