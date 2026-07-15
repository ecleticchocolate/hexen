//@ expect val 42
struct Wrap[T]{ T v }
impl Wrap[T] { fn get() T { return self.v } }
fn unwrap[S](S x) i32 {
    match S {
        P* { Wrap[P] w  w.v = *x  return (i32)w.get() }
        else { return -1 }
    }
}
fn main() i32 { i32 n = 42  i32* p = &n  return unwrap(p) }
