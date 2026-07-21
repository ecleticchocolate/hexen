//@ expect stdout
//@ | sizeof captured E   : 4
//@ | construct Box[E]*N  : 5
//@ | swapped field shape : 8
extern fn printf(u8* fmt, ...) i32
struct Box[T] { T v }
fn unwrapped_size[T]() u32 {
    match T {
        Box[E] { return sizeof(E) }
        else   { return 0 }
    }
}
fn rebuilt[T]() u32 {
    match T {
        E[N] {
            Box[E] b
            return sizeof(b) * N
        }
        else { return 0 }
    }
}
fn swap_shape[T]() u32 {
    match T {
        struct { A; B } {
            struct { B x  A y } flipped
            return sizeof(flipped)
        }
        else { return 0 }
    }
}
fn main() i32 {
    printf("sizeof captured E   : %d\n", unwrapped_size[Box[i32]]())
    printf("construct Box[E]*N  : %d\n", rebuilt[u8[5]]())
    printf("swapped field shape : %d\n", swap_shape[struct{ i32; u8 }]())
    return 0
}
