//@ expect stdout
//@ | a=1 b=2
//@ | literal in string: "{not a brace}"
extern fn printf(u8* fmt, ...) i32;
enum Option[T] { T Some None }
struct Deep { i32 a; i32 b; }

fn main() i32 {
    Option[Deep] x = .Some({ .a = 1, .b = 2 })
    // deliberately deeply nested payload before the arm body brace
    match x {
        .Some({ .a = aa, .b = bb }) {
            // this comment has a brace } in it, shouldn't confuse anything
            printf("a=%d b=%d\n", aa, bb)
        }
        .None { printf("none\n") }
    }

    // string literal containing braces right before arm body
    Option[i32] y = .None
    match y {
        .Some(v) { printf("s{o}me: %d\n", v) }
        .None { printf("literal in string: \"{not a brace}\"\n") }
    }
    return 0
}
