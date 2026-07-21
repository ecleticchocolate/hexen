//@ expect stdout
//@ | array dim  : 52
//@ | reflect dim: 12
//@ | reflect arg: 2
extern fn printf(u8* fmt, ...) i32
fn fib(u32 n) u32 { if n < 2 { return n }  return fib(n-1) + fib(n-2) }
fn nf[T]() u32 {
    match T {
        struct { H; Rest... } { return 1 + nf[Rest]() }
        struct {  } { return 0 }
        else { return 0 }
    }
}
const u32 ARR_DIM = fib(7)
fn main() i32 {
    u32[ARR_DIM] arr
    printf("array dim  : %d\n", sizeof(arr))
    const u32 REFLECT_DIM = nf[struct{ i32; i32; i32 }]()
    u32[REFLECT_DIM] slots
    printf("reflect dim: %d\n", sizeof(slots))
    u32 t1
    const { t1 = nf[struct{ i32; u8 }]() }
    printf("reflect arg: %d\n", t1)
    return 0
}
