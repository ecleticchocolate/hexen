//@ expect val 42
fn main() i32 {
    i32[3] a = {1, 2, 3}
    i32* p = &a[0]
    *(p+1) = 42
    return a[1]
}
