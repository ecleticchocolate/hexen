//@ expect stdout
//@ | f32 arm: 1
//@ | f64 arm: 2
//@ | i32 arm: 3
extern fn printf(u8* fmt, ...) i32
fn kind[T]() u32 {
    match T {
        f32 { return 1 }
        f64 { return 2 }
        i32 { return 3 }
        else { return 0 }
    }
}
fn main() i32 {
    printf("f32 arm: %d\n", kind[f32]())
    printf("f64 arm: %d\n", kind[f64]())
    printf("i32 arm: %d\n", kind[i32]())
    return 0
}
