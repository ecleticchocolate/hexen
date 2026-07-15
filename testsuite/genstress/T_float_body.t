//@ expect val 9
fn sf[T](T x) T {
    T[3] arr
    arr[0] = x * x; arr[1] = x + x; arr[2] = x - (x / (x + x))
    T acc = 0.0
    for u32 i = 0 to 3 { acc = acc + arr[i] }
    return acc
}
fn main() i32 { return (i32)sf((f64)2.0) }
