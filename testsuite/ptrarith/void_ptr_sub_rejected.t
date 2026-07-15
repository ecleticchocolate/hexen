//@ expect err pointer arithmetic on void*
fn main() i32 {
    i32 x = 42
    i32 y = 7
    void* p = (void*) &x
    void* q = (void*) &y
    i64 d = p - q
    return (i32) d
}
