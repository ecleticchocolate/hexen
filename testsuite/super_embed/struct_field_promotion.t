//@ expect val 49
// `super A base` inside a struct body embeds A's fields into the struct,
// promoted to top level (b.x reaches the embedded field directly) AND under
// the given name (b.base gives the whole embedded A back, as a second,
// independently-stored copy -- writes through the promoted name do not alias
// writes through `.base`; see field_promotion_and_base_are_independent_copies).
struct A {
   u32 x
}

struct B {
   super A base
   u32 y
}

fn main() i32 {
    B b
    b.x = 42
    b.y = 7
    return b.x + b.y
}
