//@ expect val 327
struct Pt { i32 x i32 y }
fn show(Pt p, i32 b, i32 c, i32 d, i32 e, i32 f, i32 g) i32 {
    return p.x+p.y+b+c+d+e+f+g
}
fn main() i32 {
    Pt pt = {.x = 100, .y = 200}
    return show(pt, 2,3,4,5,6,7)
}
