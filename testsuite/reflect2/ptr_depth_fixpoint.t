//@ expect stdout
//@ | depth: 3
extern fn printf(u8* fmt, ...) i32
fn ptr_depth[T]() u32 {
    match T {
        P*   { return 1 + ptr_depth[P]() }
        else { return 0 }
    }
}
fn main() i32 {
    printf("depth: %d\n", ptr_depth[i32***]())
    return 0
}
