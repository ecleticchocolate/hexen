//@ expect val 2
struct P { u8 a  u8 b }
fn w[T, u64 OFF]() u64 {
    match T {
        struct { H; Rest... } { return w[Rest, OFF + 1]() }
        struct {  } { return OFF }
    }
}
fn main() i32 { return (i32) w[P, 0]() }
