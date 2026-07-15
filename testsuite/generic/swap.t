//@ expect val 21
fn swap[T](T* a, T* b) { T tmp = *a; *a = *b; *b = tmp }
fn main() i32 {
    i32 x = 1
    i32 y = 2
    swap(&x, &y)
    return x * 10 + y
}
