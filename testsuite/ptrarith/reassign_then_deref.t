//@ expect val 30
fn main() i32 {
    i32[3] arr = {10, 20, 30}
    i32* p = &arr[0]
    p = p + 2
    return *p
}
