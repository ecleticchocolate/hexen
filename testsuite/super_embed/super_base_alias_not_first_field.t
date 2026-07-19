//@ expect val 46
// The alias offset is the prefix start, which need NOT be offset 0 when `super`
// isn't the first field. Struct_Layout points the packaged field at
// fields[i - span].offset, so b.base.x correctly aliases b.x wherever the prefix
// lands (here after b.head).
struct A { u32 x }
struct B { u32 head  super A base  u32 tail }
fn main() i32 {
    B b
    b.head = 1
    b.x = 42
    b.tail = 3
    return b.head + b.base.x + b.tail   // 1 + 42 + 3
}
