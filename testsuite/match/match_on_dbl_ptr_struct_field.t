//@ expect val 15
enum Tag { u32 A  u32 B }
struct Item { Tag t  u32 val }
fn main() i32 {
    Item it = {.t = .A{10}, .val = 5}
    Item* p = &it
    Item** pp = &p
    match (*pp).t {
        .A{v} { return (i32) v + (i32) (*pp).val }
        .B{v} { return -1 }
    }
    return -2
}
