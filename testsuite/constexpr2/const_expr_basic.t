//@ expect stdout
//@ | arg   : 55
//@ | cond  : yes
//@ | mixed : 15
//@ | float : 6.000000
//@ | nested: 20
extern fn printf(u8* fmt, ...) i32
fn fib(u32 n) u32 { if n < 2 { return n }  return fib(n-1) + fib(n-2) }
fn main() i32 {
    u32 t1
    const { t1 = fib(10) }
    printf("arg   : %d\n", t1)

    u32 t2
    const { t2 = 2 + 3 }
    if t2 > 4 { printf("cond  : yes\n") }

    u32 r = 7
    u32 t3
    const { t3 = fib(6) }
    printf("mixed : %d\n", r + t3)

    f64 t4
    const { t4 = 1.5 * 4.0 }
    printf("float : %f\n", t4)

    u32 t5
    const { t5 = (2 + 3) * 4 }
    printf("nested: %d\n", t5)
    return 0
}
