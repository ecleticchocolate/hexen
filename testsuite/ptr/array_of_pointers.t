//@ expect val 20
fn main() i32 {
    u32 a = 10
    u32 b = 20
    u32 c = 30
    u32*[3] ptrs = {&a, &b, &c}
    return (i32) *ptrs[1]
}
