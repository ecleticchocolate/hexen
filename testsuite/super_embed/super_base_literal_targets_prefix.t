//@ expect val 16
// Constructing through the packaged name `.base = {...}` writes into the shared
// promoted prefix (single storage), so the promoted fields read those values
// back. This is the initializer side of the aliasing: b.base and b.x/b.z are the
// same bytes whether written through the prefix or the packaged name.
struct A { u32 x  u32 z }
struct B { super A base  u32 y }
fn main() i32 {
    B b = {.base = {.x = 7, .z = 8}, .y = 1}
    return b.x + b.z + b.y   // 7 + 8 + 1
}
