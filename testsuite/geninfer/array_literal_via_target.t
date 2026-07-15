//@ expect val 60
fn id[T](T v) T { return v }
fn main() i32 {
    u32[3] a = id({10, 20, 30})
    return (i32)(a[0] + a[1] + a[2])
}
