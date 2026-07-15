//@ expect stdout
//@ | zero!
//@ | nonzero: 5
extern fn printf(u8* fmt, ...) i32;
enum Option[T] { T Some None }
fn main() i32 {
    Option[i32] a = .Some{0}
    Option[i32] b = .Some{5}
    // literal check INSIDE the payload position, not just a bind
    match a {
        .Some{0} { printf("zero!\n") }
        .Some{v} { printf("nonzero: %d\n", v) }
        .None { printf("none\n") }
    }
    match b {
        .Some{0} { printf("zero!\n") }
        .Some{v} { printf("nonzero: %d\n", v) }
        .None { printf("none\n") }
    }
    return 0
}
