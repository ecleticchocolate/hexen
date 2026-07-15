//@ expect stdout
//@ | sizeof b: 20
extern fn printf(u8* fmt, ...) i32

struct Config { u32 count }

struct Buffer[Config C] {
    u32[C.count] data
}

fn main() i32 {
    Buffer[{.count=5}] b
    printf("sizeof b: %d\n", sizeof(b)) // Should be 20 (5 * 4)
    return 0
}
