//@ expect val 8
struct Box[T] { T val }
fn main() i32 {
    match Box[u64] {
        Box[E] { return (i32)sizeof(E) }
        else { return 9 }
    }
}
