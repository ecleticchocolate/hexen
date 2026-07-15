//@ expect val 21
fn make_arr[T](T seed) T[3] {
    T[3] a
    a[0] = seed
    a[1] = seed
    a[2] = seed
    return a
}
fn main() u32 {
    u32[3] r = make_arr(7)
    return r[0] + r[1] + r[2]
}
