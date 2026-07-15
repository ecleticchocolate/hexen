//@ expect val 13
struct Pair[A,B] { A first  B second }
impl Pair[A,B] {
    fn combine[C](C extra) i32 { return (i32)self.first + extra }
}
fn main() i32 {
    Pair[i32,u32] p = {.first = 3, .second = 4}
    return p.combine((i32)10)
}
