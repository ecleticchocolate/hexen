//@ expect val 49
// `super Box[i32] base` -- the embedded type is a concrete generic
// instantiation, not a bare name. Requires going through the real type
// parser (parse_type) rather than a single-identifier lookup.
struct Box[T] { T val }
struct B {
    super Box[i32] base
    u32 y
}
fn main() i32 {
    B b
    b.val = 42
    b.y = 7
    return b.val + b.y
}
