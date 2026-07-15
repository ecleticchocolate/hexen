//@ expect val 99
fn set_ptr(u32** pp, u32* target) { *pp = target }
fn main() i32 {
    u32 a = 1
    u32 b = 99
    u32* p = &a
    set_ptr(&p, &b)
    return (i32) *p
}
