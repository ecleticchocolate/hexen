//@ expect val 4950
fn main() i32 {
    u32[100] a
    u32 i = 0
    while i < 100 { a[i] = i  i = i + 1 }
    u32 s = 0
    i = 0
    while i < 100 { s = s + a[i]  i = i + 1 }
    return (i32) s
}
