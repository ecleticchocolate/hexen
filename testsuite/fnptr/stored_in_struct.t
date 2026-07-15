//@ expect val 21
struct Cb { fn(u32) u32 f }
fn triple(u32 x) u32 { return x * 3 }
fn main() i32 {
    Cb c = {.f = triple}
    fn(u32) u32 fp = c.f
    return (i32) fp(7)
}
