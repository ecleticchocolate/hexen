//@ expect stdout
//@ | first=1, x=2 y=3 z=4 len=4
extern fn printf(u8* fmt, ...) i32;

struct Buf[T, u32 N] { T[N] data; u32 len; }

fn main() i32 {
    Buf[i32, 4] b = { .data = {1,2,3,4}, .len = 4 }
    match b {
        { .data = { 1, x, y, z }, .len = l } {
            printf("first=1, x=%d y=%d z=%d len=%u\n", x, y, z, l)
        }
        else { printf("no match\n") }
    }
    return 0
}
