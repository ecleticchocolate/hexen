//@ expect val 21
struct Box[T]{ T v }
struct S { Box[u16] b  u8 t }
fn main() i32 {
    match S {
        struct { Box[E] bb  F tt } { return (i32)sizeof(E)*10 + (i32)sizeof(F) }
        else { return 99 }
    }
}
