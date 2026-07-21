//@ expect stdout
//@ | box field : 1
//@ | arr field : 2
//@ | ptr field : 3
//@ | fn  field : 4
extern fn printf(u8* fmt, ...) i32
struct Box[T] { T v }
fn describe[T]() u32 {
    match T {
        struct { Box[E] } { return 1 }
        struct { E[N] }   { return 2 }
        struct { P* }     { return 3 }
        struct { fn(A) B }{ return 4 }
        else { return 0 }
    }
}
fn main() i32 {
    printf("box field : %d\n", describe[struct{ Box[i32] }]())
    printf("arr field : %d\n", describe[struct{ u8[4] }]())
    printf("ptr field : %d\n", describe[struct{ i32* }]())
    printf("fn  field : %d\n", describe[struct{ fn(i32) u8 }]())
    return 0
}
