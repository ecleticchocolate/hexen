//@ expect val 77
fn main() i32 {
    u32[8] arr
    u32[8]* p = &arr
    p[5] = 77
    return (i32) arr[5]
}
