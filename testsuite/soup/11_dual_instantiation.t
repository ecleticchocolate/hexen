//@ expect stdout
//@ | a: 42
//@ | b: 7
extern fn printf(u8* fmt, ...) i32;
enum Option[T] { T Some None }
fn main() i32 {
    Option[i32] a = .Some(42)
    Option[u8] b = .Some(7)
    match a { .Some(v) { printf("a: %d\n", v) } .None { printf("a: none\n") } }
    match b { .Some(v) { printf("b: %u\n", v) } .None { printf("b: none\n") } }
    return 0
}
