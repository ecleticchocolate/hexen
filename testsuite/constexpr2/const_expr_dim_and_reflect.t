//@ expect stdout
//@ | array dim  : 52
//@ | reflect dim: 12
//@ | reflect arg: 2
extern fn printf(u8* fmt, ...) i32
fn fib(u32 n) u32 { if n < 2 { return n }  return fib(n-1) + fib(n-2) }
fn nf[T]() u32 {
    match T {
        struct { H h  Rest... r } { return 1 + nf[Rest]() }
        struct {} { return 0 }
        else { return 0 }
    }
}
fn main() i32 {
    u32[const(fib(7))] arr
    printf("array dim  : %d\n", sizeof(arr))
    u32[const(nf[struct{i32 a  i32 b  i32 c}]())] slots
    printf("reflect dim: %d\n", sizeof(slots))
    printf("reflect arg: %d\n", const(nf[struct{i32 a  u8 b}]()))
    return 0
}
