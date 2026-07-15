//@ expect val 15
struct Big { i32 a  i32 b  i32 c  i32 d  i32 e }
fn make_big() Big { return {.a = 1, .b = 2, .c = 3, .d = 4, .e = 5} }
fn main() i32 { Big b = make_big(); return b.a + b.b + b.c + b.d + b.e }
