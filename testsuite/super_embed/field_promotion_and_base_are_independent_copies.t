//@ expect val 149
// This is the simplest-possible implementation's real limitation, pinned
// deliberately rather than hidden: `super A base` splices a COPY of A's
// fields as top-level fields of B, plus a SEPARATE copy of A under the name
// `base`. They are not the same storage. Writing through the promoted name
// (b.x) does not change what b.base.x reads back.
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
    // b.base.x / b.base.z are still 0 -- an independent, never-written copy.
    return b.x + b.z + b.y + b.base.x + b.base.z
}
