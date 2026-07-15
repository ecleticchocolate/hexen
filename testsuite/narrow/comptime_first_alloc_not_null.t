//@ expect val 2
struct T { u32 x }
fn check(T* p) u32 {
    if p == null { return 1 }
    return 2
}
fn build() u32 {
    T* p = new T{.x = 5}
    return check(p)
}
const u32 TOTAL = build()
fn main() i32 { return (i32)TOTAL }
