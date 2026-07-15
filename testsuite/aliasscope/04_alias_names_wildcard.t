//@ expect val 8
struct Box[T] { T v }
fn probe[T](T x) i32 {
    match T {
        Box[E] { alias Elem = E  return (i32)sizeof(Elem) }
        else { return 0 }
    }
}
fn main() i32 { Box[u64] b = { .v = 1 }  return probe(b) }
