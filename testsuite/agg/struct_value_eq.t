//@ expect val 11
struct P { u32 x  u32 y }
fn main() u32 {
    P a = {.x = 1, .y = 2}
    P b = {.x = 1, .y = 2}
    P c = {.x = 1, .y = 3}
    u32 r = 0
    if a == b { r = r + 1 }
    if a != c { r = r + 10 }
    if a != b { r = r + 100 }
    return r
}
