//@ expect val 104
enum Tag { u32 A  u32 B }
struct Item { Tag t  u32 val }
fn main() i32 {
    Item it = {.t = .B(99), .val = 5}
    Item* p = &it
    Item** pp = &p
    match (*pp).t {
        .A(v) { return (i32) v }
        .B(v) { return (i32) v + (i32) (*pp).val }
    }
    return -1
}
