//@ expect err not bound by this generic's parameter list
struct P { u8 a  u8 b }
fn w[T, u64 OFF]() u64 {
    match T {
        struct { H; Rest... } { return w[Rest, OFF + sizeof(H)]() }
        struct {  } { return OFF }
    }
}
fn main() i32 { return (i32) w[P, 0]() }
