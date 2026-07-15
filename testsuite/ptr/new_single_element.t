//@ expect val 42
fn main() i32 {
    u32* p = new[1] u32
    *p = 42
    i32 r = (i32) *p
    delete p
    return r
}
