//@ expect val 0
struct Mask { i32 bits }
impl Mask {
    fn __bitnot() i32 { return ~self.bits }
}
fn main() i32 {
    Mask m = { .bits = 5 }
    if (~m != -6) { return 1 }
    return 0
}
