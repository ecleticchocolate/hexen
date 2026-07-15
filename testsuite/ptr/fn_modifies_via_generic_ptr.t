//@ expect val 0
fn zero[T](T* p) { *p = (T) 0 }
fn main() i32 {
    u32 x = 99
    zero(&x)
    return (i32) x
}
