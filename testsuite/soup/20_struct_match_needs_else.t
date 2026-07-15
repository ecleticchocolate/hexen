//@ expect err not exhaustive
extern fn printf(u8* fmt, ...) i32;
struct P { i32 x  i32 y }
fn main() i32 {
    P p = { .x = 9, .y = 2 }
    match p {
        { .x = 1, .y = yy } { printf("one %d\n", yy) }
        { .x = xx, .y = yy } { printf("%d %d\n", xx, yy) }
    }
    return 0
}
