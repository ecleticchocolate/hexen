//@ expect val 42
struct T { i32 x }
fn build() i32 {
    T a = {.x = 42}
    T b = a
    b.x = 999
    return a.x
}
const i32 R = build()
fn main() i32 { return R }
