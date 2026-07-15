//@ expect val 6
fn main() i32 {
    u32[8] arr
    u32 i = 0
    while i < 8 { arr[i] = i * 2  i = i + 1 }
    u32[8]* p = &arr
    return (i32) p[3]
}
