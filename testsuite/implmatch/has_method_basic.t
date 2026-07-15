//@ expect stdout
//@ | A (has free):
//@ |   A.free called
//@ | B (no free):
//@ |   no free method
extern fn printf(u8* fmt, ...) i32
struct A { u32 x }
impl A { fn free() void { printf("  A.free called\n") } }
struct B { u32 x }
fn cleanup[T](T v) {
    match T {
        impl { fn free() } { v.free() }
        else { printf("  no free method\n") }
    }
}
fn main() i32 {
    A a = {.x=1}
    B b = {.x=2}
    printf("A (has free):\n")   cleanup(a)
    printf("B (no free):\n")    cleanup(b)
    return 0
}
