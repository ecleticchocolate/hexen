//@ expect val 6
fn make_arr[T](T seed) T[3] {
    T[3] a
    a[0] = seed
    a[1] = seed
    a[2] = seed
    return a
}
fn main() i32 {
    f64[3] r = make_arr(2.0)
    return (i32)(r[0] + r[1] + r[2])
}
