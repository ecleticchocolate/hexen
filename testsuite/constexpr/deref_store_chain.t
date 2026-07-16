//@ expect val 0
struct Inner { i64 d }
struct Outer { Inner* b }

fn build() i64 {
    Inner i = { .d = 0 }
    Outer o = { .b = &i }
    Outer* p = &o
    (*p).b.d = 99
    return i.d
}
const i64 R = build()
fn main() i32 { if (R != 99) { return 1 }  return 0 }
