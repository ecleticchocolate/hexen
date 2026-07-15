//@ expect val 99
fn main() i32 {
    u32[4] arr = {1, 2, 3, 4}
    u32* p = &arr[2]
    *p = 99
    return (i32) arr[2]
}
