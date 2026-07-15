//@ expect val 42
fn deref_it[T](T* p) T { return *p }
fn main() i32 {
    i32 x = 42
    return deref_it(&x)
}
