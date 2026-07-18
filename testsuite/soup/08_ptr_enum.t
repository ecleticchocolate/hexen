//@ expect stdout
//@ | B: 99 val=5
extern fn printf(u8* fmt, ...) i32;
enum Tag { u32 A  u32 B }
struct Item { Tag t; u32 val; }
fn main() i32 {
    Item it = { .t = .B(99), .val = 5 }
    Item* p = &it
    Item** pp = &p
    match (*pp).t {
        .A(v) { printf("A: %u\n", v) }
        .B(v) { printf("B: %u val=%u\n", v, (*pp).val) }
    }
    return 0
}
