//@ expect val 99
struct T { u32 x }
fn touch(T* p) {
    if p == null { return }
    p.x = 99
}
fn build() u32 {
    T t
    t.x = 0
    touch(&t)
    return t.x
}
const u32 TOTAL = build()
fn main() i32 { return (i32)TOTAL }
