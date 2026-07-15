//@ expect stdout
//@ | comptime float loop : 8.500000
//@ | comptime float heap : 4.000000
extern fn printf(u8* fmt, ...) i32
fn fsum(f64[5] a) f64 {
    f64 acc = 0.0
    u32 i = 0
    while i < 5 { acc = acc + a[i]  i = i + 1 }
    return acc
}
struct V { f64 x  f64 y }
fn heaped() f64 {
    V* p = new V{.x = 1.25, .y = 2.75}
    f64 r = p.x + p.y
    delete p
    return r
}
fn main() i32 {
    const f64 S = fsum({1.5, 2.5, 3.0, 0.5, 1.0})
    const f64 H = heaped()
    printf("comptime float loop : %f\n", S)
    printf("comptime float heap : %f\n", H)
    return 0
}
