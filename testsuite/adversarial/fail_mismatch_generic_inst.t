//@ expect err type mismatch in assignment: cannot assign Box[u32][2] to Box[i32][2]
struct Box[T] { T val }
fn main() i32 {
    Box[u32][2] g1 = {{.val=1}, {.val=2}}
    Box[i32][2] g2
    g2 = g1
    return 0
}
