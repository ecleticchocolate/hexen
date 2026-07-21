//@ expect val 1
// A SECOND bracket after the argument group is ordinary postfix, not another
// argument -- identical to a concrete head, where `Box[E][N]` is array-of-Box[E].
// Multi-argument templates use the comma list, so `[` never changes meaning by
// position. This is the one production that previously disagreed.
struct Box[T] { T v }
fn p[T]() i32 {
    match T { struct M[E][N] { return 1 } else { return 3 } }
    return -1
}
fn main() i32 { return p[Box[u8][4]]() }
