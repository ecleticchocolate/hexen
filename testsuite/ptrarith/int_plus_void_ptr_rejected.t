//@ expect err pointer arithmetic on void*
fn main() i32 {
    i32 x = 42
    void* p = (void*) &x
    void* q = 1 + p
    return 0
}
