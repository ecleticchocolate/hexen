//@ expect val 42
struct P { u32 v }
enum E { u32 A  None }
fn pick(E e) P {
    match e { .A{x} { return {.v = x} }  .None { return {.v = 0} } }
    return {.v = 99}
}
fn main() i32 { return (i32) pick(.A{42}).v }
