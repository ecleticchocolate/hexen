//@ expect val 1
struct P { u32 x  u32 y }
fn classify(P p) i32 {
    match p {
        {.x=0, .y=0} { return 0 }
        {.x=1, .y=2} { return 1 }
        else { return -1 }
    }
}
fn main() i32 {
    P p1 = {.x=1, .y=2}
    return classify(p1)
}
