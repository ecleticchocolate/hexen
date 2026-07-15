//@ expect val 10
fn first_ptr[T](T* arr) T* { return arr }
fn main() i32 {
    u32[3] a = {10, 20, 30}
    u32* p = first_ptr(&a[0])
    return (i32) *p
}
