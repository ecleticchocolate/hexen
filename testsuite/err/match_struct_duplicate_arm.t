//@ expect err duplicate match arm
struct P { u32 x  u32 y }
fn main() i32 {
    P p = {.x=1,.y=2}
    match p {
        {.x=1,.y=2} { return 1 }
        {.x=1,.y=2} { return 2 }
        else { return 0 }
    }
    return 0
}
