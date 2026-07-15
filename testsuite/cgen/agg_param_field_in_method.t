//@ expect val 24
struct Cfg { u32 w  u32 h  u32 d }
struct E[T, Cfg C] { T t }
impl E[T, Cfg C] { fn vol() u32 { return C.w * C.h * C.d } }
fn main() i32 {
    E[i32, {.w=4, .h=3, .d=2}] e = {.t=9}
    return (i32)e.vol()   // 24
}
