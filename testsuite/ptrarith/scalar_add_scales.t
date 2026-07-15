//@ expect val 20
fn main() i32 {
    i32[3] arr = {10, 20, 30}
    i32* p = &arr[0]
    i32* q = p + 1
    return *q
}
