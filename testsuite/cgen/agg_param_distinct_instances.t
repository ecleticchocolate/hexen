//@ expect val 49
struct Cfg { u32 w  u32 h  u32 d }
struct E[T, Cfg C] { T t }
impl E[T, Cfg C] { fn vol() u32 { return C.w * C.h * C.d } }
fn main() i32 {
    E[i32, {.w=4, .h=3, .d=2}] a = {.t=1}
    E[i32, {.w=5, .h=5, .d=1}] b = {.t=1}
    return (i32)(a.vol() + b.vol())   // 24 + 25 = 49
}
