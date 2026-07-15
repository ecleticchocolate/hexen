//@ expect val 3
fn main() i32 {
    i32[5] arr = {1, 2, 3, 4, 5}
    i32* p0 = &arr[0]
    i32* p3 = &arr[3]
    i64 dist = p3 - p0
    return (i32) dist
}
