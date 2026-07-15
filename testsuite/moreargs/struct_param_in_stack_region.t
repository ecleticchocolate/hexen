//@ expect val 321
struct Pt { i32 x i32 y }
fn show(i32 a, i32 b, i32 c, i32 d, i32 e, i32 f, Pt p) i32 {
    return a+b+c+d+e+f+p.x+p.y
}
fn main() i32 {
    Pt pt = {.x = 100, .y = 200}
    return show(1,2,3,4,5,6, pt)
}
