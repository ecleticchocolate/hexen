//@ expect stdout
//@ | 1 2 9
extern fn printf(u8* fmt, ...) i32;
struct Inner { i32 a  i32 b }
struct Outer { Inner in  i32 tag }
fn main() i32 {
    Outer o = { .in = { .a = 1, .b = 2 }, .tag = 9 }
    match o {
        { .in = { .a = aa, .b = bb }, .tag = t } { printf("%d %d %d\n", aa, bb, t) }
        else { printf("no\n") }
    }
    return 0
}
