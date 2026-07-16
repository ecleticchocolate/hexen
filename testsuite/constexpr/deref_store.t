//@ expect val 0
fn build() i64 {
    i64* p = new i64
    *p = 42
    return *p
}
const i64 R = build()
fn main() i32 { if (R != 42) { return 1 }  return 0 }
