//@ expect val 42
fn main() i32 {
    i32 x = 42
    i32* p = &x
    u64 addr = (u64) p
    i32* p2 = (i32*) addr
    return *p2
}
