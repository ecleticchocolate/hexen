//@ expect err not exhaustive
struct P { u32 x  u32 y }
fn main() i32 {
    P p = {.x=1,.y=2}
    match p {
        {.x=1,.y=2} { return 1 }
    }
    return 0
}
