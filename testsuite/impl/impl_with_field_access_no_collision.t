//@ expect val -7
struct Pair { i32 a  i32 b }
impl Pair {
    fn swap() { i32 t = self.a  self.a = self.b  self.b = t }
    fn diff() i32 { return self.a - self.b }
}
fn main() i32 {
    Pair p = {.a = 10, .b = 3}
    p.swap()
    return p.diff()
}
