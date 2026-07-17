//@ expect val 50
// Regression: for-in must resolve __index/len through the generic base name
// (Box_len, not Box_i32_len) for an INSTANTIATED generic struct, same as any
// other method call does via Method_Mangle.
struct Box[T] { T[4] d }
impl Box[T] {
    fn __index(u32 i) T* { return &self.d[i] }
    fn len() u64 { return 4 }
}
fn main() i32 {
    Box[i32] b = { .d = {5, 10, 15, 20} }
    i32 s = 0
    for i32 x in b { s = s + x }
    return s
}
