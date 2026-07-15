//@ expect val 43
fn identity[T](T x) T { return x }
fn main() i32 {
    i32 a = identity(10)
    bool b = identity(true)
    u64 c = identity((u64) 32)
    return a + (i32) b + (i32) c
}
