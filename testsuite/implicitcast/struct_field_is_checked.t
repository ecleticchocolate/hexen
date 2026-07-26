//@ expect err type mismatch in struct field
// A TYPED struct-literal field value was never type-checked: `H{.a = b}`
// accepted an unrelated struct -- even one of a different SIZE -- and simply
// reinterpreted its bytes. Only untyped literals were checked.
struct A { u32 x }
struct B { u32 y }
struct H { A a }
fn main() i32 {
    B b
    b.y = 5
    H h = {.a = b}
    return (i32)h.a.x
}
