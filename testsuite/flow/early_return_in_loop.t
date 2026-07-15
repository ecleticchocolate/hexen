//@ expect val 2
fn first_gt(u32* arr, u32 n, u32 threshold) i32 {
    for u32 i = 0 to n {
        if arr[i] > threshold { return (i32) i }
    }
    return -1
}
fn main() i32 {
    u32[5] a = {1, 2, 10, 3, 4}
    return first_gt(&a[0], 5, 5)
}
