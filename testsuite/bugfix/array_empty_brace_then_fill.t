//@ expect val 14
fn fill[T](T* arr, u32 n, T val) {
    u32 i = 0
    while i < n { arr[i] = val  i = i + 1 }
}
fn main() i32 {
    i32[4] buf = {}
    fill(&buf[0], 4, 7)
    return buf[0] + buf[3]
}
