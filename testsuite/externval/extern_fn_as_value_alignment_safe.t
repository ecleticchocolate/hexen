//@ expect val 48
extern fn abs(i32 n) i32
fn main() i32 {
    i32 pad1 = 1
    fn(i32) i32 fp = abs
    i32 pad2 = 2
    i32 pad3 = 3
    return fp(-42) + pad1 + pad2 + pad3
}
