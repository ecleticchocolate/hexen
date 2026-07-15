//@ expect stdout
//@ | struct: 1 3
//@ | array: 7
extern fn printf(u8* fmt, ...) i32;
struct P { i32 x  i32 y  i32 z }
fn main() i32 {
    P p = { .x = 1, .y = 2, .z = 3 }
    match p { { .x = a, .y = _, .z = c } { printf("struct: %d %d\n", a, c) } else { printf("no\n") } }
    i32[3] arr = { 5, 6, 7 }
    match arr { { 5, _, last } { printf("array: %d\n", last) } else { printf("no\n") } }
    return 0
}
