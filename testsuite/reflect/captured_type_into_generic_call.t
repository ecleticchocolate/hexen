//@ expect val 42
fn dbl[U](U x) U { return x + x }
fn viamatch[T](T x) i32 {
    match T {
        P* { P v = *x  P r = dbl(v)  return (i32)r }
        else { return -1 }
    }
}
fn main() i32 { i32 n = 21  i32* p = &n  return viamatch(p) }
