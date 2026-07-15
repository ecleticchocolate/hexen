//@ expect val 42
struct S { u32[2] arr  u32 k }
fn main() i32 {
    S s = {{10, 20}, 12}
    return (i32)(s.arr[0] + s.arr[1] + s.k)
}
