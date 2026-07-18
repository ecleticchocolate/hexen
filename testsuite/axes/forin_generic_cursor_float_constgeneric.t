//@ expect val 22
// Axis cross: FLOAT const-generic value param (f64 K) on the struct that
// drives the for-in CURSOR protocol -- the cursor struct itself carries a
// float value-generic, scaling each yielded element.
enum Option[T] { T Some  None }
struct Scaler[f64 K] { i32[3] items  u32 pos }
struct Cur[f64 K] { i32[3] items  u32 pos }
impl Scaler[f64 K] {
    fn begin() Cur[K] { return {.items = self.items, .pos = 0} }
}
impl Cur[f64 K] {
    fn next() Option[i32] {
        if self.pos >= 3 { return .None }
        i32 v = (i32)((f64)self.items[self.pos] * K)
        self.pos = self.pos + 1
        return .Some(v)
    }
}
fn main() i32 {
    Scaler[2.0] s = {.items = {1, 2, 8}, .pos = 0}
    i32 sum = 0
    for i32 x in s { sum = sum + x }
    return sum
}
