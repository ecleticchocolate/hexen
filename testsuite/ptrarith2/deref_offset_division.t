//@ expect val 99
fn main() i32 {
    i32[3] a = {1, 2, 3}
    i32* p = &a[0]
    *(p + (18/9)) = 99
    return a[2]
}
