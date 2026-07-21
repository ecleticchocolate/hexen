//@ expect val 7
struct Box[T] { T v }
fn p[S]() i32 {
    match S {
        struct M[X] { match X { i32 { return 7 } else { return 8 } } }
        else { return 2 }
    }
    return -1
}
fn main() i32 { return p[Box[i32]]() }
