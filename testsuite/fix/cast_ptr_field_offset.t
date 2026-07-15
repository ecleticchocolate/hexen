//@ expect val 0
extern fn malloc(u64 n) u8*
struct Foo { u32 x  i64 y }
fn main() i32 {
    u8* raw = (u8*)malloc(24)
    Foo* f = (Foo*)raw
    f.x = 99
    f.y = 12345
    if f.x != 99 { return 1 }
    if f.y != 12345 { return 2 }
    return 0
}
