//@ expect val 1324868
struct P { u32 x  u32 y }
fn main() u32 {
    P a = {.x = 10, .y = 20}
    P b = {.x = 3, .y = 4}
    P s = a + b
    P d = a - b
    P m = a * b
    return s.x * 100000 + s.y * 1000 + d.x * 100 + d.y * 10 + m.y / 10
}
