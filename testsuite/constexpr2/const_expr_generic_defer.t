//@ expect stdout
//@ | N=5 double: 10
//@ | N=5 plus  : 6
//@ | N=8 double: 16
//@ | sizeof i32 x2: 8
//@ | sizeof f64 x2: 16
extern fn printf(u8* fmt, ...) i32
struct Buf[T, u32 N] { T[N] data }
impl Buf[T, N] {
    fn double_cap() u32 { return const(N * 2) }
    fn cap_plus() u32   { return const(N + 1) }
}
fn tsize[T]() u32 { return const(sizeof(T) * 2) }
fn main() i32 {
    Buf[i32, 5] a
    Buf[i32, 8] b
    printf("N=5 double: %d\n", a.double_cap())
    printf("N=5 plus  : %d\n", a.cap_plus())
    printf("N=8 double: %d\n", b.double_cap())
    printf("sizeof i32 x2: %d\n", tsize[i32]())
    printf("sizeof f64 x2: %d\n", tsize[f64]())
    return 0
}
