//@ expect err aggregate operands
struct P { u32 x  u32 y }
fn main() u32 {
    P a = {.x = 1, .y = 2}
    P b = {.x = 3, .y = 4}
    if a > b { return 1 }
    return 0
}
