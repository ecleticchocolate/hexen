//@ expect val 1
fn identity[T](T x) T { return x }
fn main() u32 {
    u32[4] a = {1, 2, 3, 4}
    if identity(a) == {1, 2, 3, 4} { return 1 }
    return 0
}
