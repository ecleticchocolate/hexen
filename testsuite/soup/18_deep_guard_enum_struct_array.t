//@ expect stdout
//@ | 9
//@ | 7
//@ | -1
extern fn printf(u8* fmt, ...) i32;
struct Vec { i32[2] xy }
enum Shape { Vec Line  None }
fn desc(Shape s) i32 {
    match s {
        .Line{{ .xy = {0, y} }} { return y }
        .Line{{ .xy = {x, y} }} { return x + y }
        .None { return -1 }
    }
}
fn main() i32 {
    printf("%d\n", desc(.Line{{ .xy = {0, 9} }}))
    printf("%d\n", desc(.Line{{ .xy = {3, 4} }}))
    printf("%d\n", desc(.None))
    return 0
}
