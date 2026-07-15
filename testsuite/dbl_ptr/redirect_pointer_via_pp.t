//@ expect val 30
fn main() i32 {
    u32[3] arr = {10, 20, 30}
    u32* p = &arr[0]
    u32** pp = &p
    *pp = &arr[2]
    return (i32) **pp
}
