//@ expect val 30
fn stress[T](T x) i32 {
    T a = x
    T b = a + a
    T* p = &a
    *p = b
    T[4] arr
    arr[0] = x; arr[1] = x + x; arr[2] = x + x + x; arr[3] = x + x + x + x
    T** pp = &p
    **pp = arr[3] - arr[2]
    i32 sum = 0
    for u32 i = 0 to 4 { sum = sum + (i32)arr[i] }
    return sum
}
fn main() i32 { return stress((i32)3) }
