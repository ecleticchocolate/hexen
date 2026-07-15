//@ expect val 7
struct Color { u8 r }
fn make_color() Color { Color c = {.r = 7}  return c }
fn main() i32 {
    (fn() Color) f = make_color
    Color got = f()
    return (i32) got.r
}
