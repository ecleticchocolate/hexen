//@ expect val 1
struct P { u32 x  u32 y }
fn main() u32 {
    P a = {.x = 1, .y = 2}
    if a == {.x = 1, .y = 2} { return 1 }
    return 0
}
