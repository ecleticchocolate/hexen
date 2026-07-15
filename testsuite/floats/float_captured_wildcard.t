//@ expect stdout
//@ | f32[3] cap: 11
//@ | f64[2] cap: 18
extern fn printf(u8* fmt, ...) i32
struct Box[T] { T v }
fn cap[T]() u32 {
    match T {
        E[N] {
            E x = (E)1
            Box[E] b
            return sizeof(x) + sizeof(b) + N
        }
        else { return 0 }
    }
}
fn main() i32 {
    printf("f32[3] cap: %d\n", cap[f32[3]]())
    printf("f64[2] cap: %d\n", cap[f64[2]]())
    return 0
}
