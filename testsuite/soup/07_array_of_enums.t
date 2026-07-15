//@ expect stdout
//@ | a=1 b=3
extern fn printf(u8* fmt, ...) i32;
enum Option[T] { T Some None }

fn main() i32 {
    Option[i32][3] arr = { .Some{1}, .None, .Some{3} }
    match arr {
        { .Some{a}, .None, .Some{b} } { printf("a=%d b=%d\n", a, b) }
        else { printf("no match\n") }
    }
    return 0
}
