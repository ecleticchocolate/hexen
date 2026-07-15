//@ expect val 32
struct Box[T] { T v }
fn f[T](T x) i32 {
    match T {
        Box[E] { alias PtrArr = E*[4]  return (i32)sizeof(PtrArr) }
        else { return 0 }
    }
}
fn main() i32 { Box[u32] b = { .v = 1 }  return f(b) }
