//@ expect err unreachable match arm
extern fn printf(u8* fmt, ...) i32;
enum Option[T] { T Some None }
fn main() i32 {
    Option[i32] a = .Some{5}
    match a {
        .Some{v} { printf("v=%d\n", v) }
        .Some{0} { printf("unreachable\n") }
        .None { printf("none\n") }
    }
    return 0
}
