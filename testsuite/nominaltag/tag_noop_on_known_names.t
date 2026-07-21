//@ expect val 21
// On a KNOWN name the tag is a pure no-op in any type position.
struct S { i32 a }
union  U { i32 i  u8* s }
fn main() i32 {
    struct S x  x.a = 2
    union  U y  y.i = 1
    return x.a * 10 + y.i
}
