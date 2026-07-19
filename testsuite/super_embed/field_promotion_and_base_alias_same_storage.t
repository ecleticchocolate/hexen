//@ expect val 291
// `super A base` gives Derived a promoted prefix (b.x, b.z) AND a packaged name
// (b.base) that ALIASES that same prefix -- single storage, not two copies.
// Writing through the promoted name (b.x) is visible through b.base.x and vice
// versa: they are the same bytes. sizeof(B) == sizeof(A) + own fields, no double.
struct A {
   u32 x
   u32 z
}

struct B {
   super A base
   u32 y
}

fn main() i32 {
    B b
    b.x = 42
    b.z = 100
    b.y = 7
    // b.base.x / b.base.z now READ BACK 42 / 100 -- same storage as the prefix.
    return b.x + b.z + b.y + b.base.x + b.base.z
}
