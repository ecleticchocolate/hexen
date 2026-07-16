//@ expect val 0
fn build() i64 {
    i64 x = 0
    i64* p = &x
    *p = 42
    return x
}
const i64 R = build()
fn main() i32 { if (R != 42) { return 1 }  return 0 }
