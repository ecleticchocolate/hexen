//@ expect val 4
struct Box[T]{ T v }
fn main() i32 {
    match Box[Box[Box[u32]]] {
        Box[A] { match A { Box[B] { match B { Box[C] { return (i32)sizeof(C) } else { return -1 } } } else { return -2 } } }
        else { return -3 }
    }
}
