//@ expect err not a constant expression
struct Box[T] { T val }
impl Box[T] {
    fn identity[U](U x) U { return x }
}
fn build() u32 {
    Box[i32] b = {.val = 5}
    return b.identity(10)
}
const u32 R = build()
fn main() i32 { return (i32)R }
