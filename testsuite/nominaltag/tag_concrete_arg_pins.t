//@ expect val 13
struct Box[T] { T v }
fn p[S]() i32 {
    match S { struct M[i32] { return 1 }  struct M[X] { return 3 }  else { return 2 } }
    return -1
}
fn main() i32 { return p[Box[i32]]()*10 + p[Box[u8]]() }
