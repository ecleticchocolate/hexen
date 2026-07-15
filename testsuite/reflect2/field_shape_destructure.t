//@ expect stdout
//@ | box field : 1
//@ | arr field : 2
//@ | ptr field : 3
//@ | fn  field : 4
extern fn printf(u8* fmt, ...) i32
struct Box[T] { T v }
fn describe[T]() u32 {
    match T {
        struct { Box[E] b } { return 1 }
        struct { E[N] a }   { return 2 }
        struct { P* p }     { return 3 }
        struct { fn(A) B f }{ return 4 }
        else { return 0 }
    }
}
fn main() i32 {
    printf("box field : %d\n", describe[struct{ Box[i32] b }]())
    printf("arr field : %d\n", describe[struct{ u8[4] a }]())
    printf("ptr field : %d\n", describe[struct{ i32* p }]())
    printf("fn  field : %d\n", describe[struct{ fn(i32) u8 f }]())
    return 0
}
