//@ expect val 42
struct T { i32 x }
fn build() i32 {
    T* p = new T{.x = 42}
    T local = *p
    return local.x
}
const i32 R = build()
fn main() i32 { return R }
