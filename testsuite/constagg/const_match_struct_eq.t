//@ expect val 100
struct P { u32 x  u32 y }
fn classify(P p) u32 {
    match p {
        {.x = 1, .y = 2} { return 100 }
        {.x = 0, .y = 0} { return 200 }
        else { return 999 }
    }
}
const u32 w = classify({.x = 1, .y = 2})
fn main() u32 { return w }
