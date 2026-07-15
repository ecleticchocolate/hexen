//@ expect stdout
//@ | arg   : 55
//@ | cond  : yes
//@ | mixed : 15
//@ | float : 6.000000
//@ | nested: 20
extern fn printf(u8* fmt, ...) i32
fn fib(u32 n) u32 { if n < 2 { return n }  return fib(n-1) + fib(n-2) }
fn main() i32 {
    printf("arg   : %d\n", const(fib(10)))
    if const(2 + 3) > 4 { printf("cond  : yes\n") }
    u32 r = 7
    printf("mixed : %d\n", r + const(fib(6)))
    printf("float : %f\n", const(1.5 * 4.0))
    printf("nested: %d\n", const(const(2+3) * 4))
    return 0
}
