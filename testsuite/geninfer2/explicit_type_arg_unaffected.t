//@ expect val 6
fn make_arr[T]() T[3] {
    T[3] a
    a[0] = 1
    a[1] = 2
    a[2] = 3
    return a
}
fn main() u32 {
    u32[3] r = make_arr()
    return r[0] + r[1] + r[2]
}
