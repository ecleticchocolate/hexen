//@ expect val 16
struct Pair[A,B] { A first  B second }
impl Pair[A,B] {
    fn get_first() A { return self.first }
    fn get_second() B { return self.second }
}
fn main() i32 {
    Pair[i32,i32] p = {.first = 7, .second = 9}
    return p.get_first() + p.get_second()
}
