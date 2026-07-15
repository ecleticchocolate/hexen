//@ expect val 8
enum Color { Red  Green  Blue }
struct Tagged { Color c  i32 val }
fn main() i32 {
    match Tagged {
        struct { C cc  V vv } { return (i32)sizeof(C) + (i32)sizeof(V) }
        else { return 0 }
    }
}
