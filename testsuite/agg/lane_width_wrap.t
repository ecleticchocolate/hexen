//@ expect val 4410
struct B { u8 p  u8 q }
fn main() u32 {
    B a = {.p = 200, .q = 5}
    B b = {.p = 100, .q = 5}
    B s = a + b
    return (u32)s.p * 100 + (u32)s.q
}
