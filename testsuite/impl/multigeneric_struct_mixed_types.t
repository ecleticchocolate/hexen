//@ expect val 7
struct Pair[A,B] { A first  B second }
impl Pair[A,B] {
    fn first_as_f32() f32 { return (f32)self.first }
}
fn main() i32 {
    Pair[i32,u32] p = {.first = 3, .second = 4}
    f32 f = p.first_as_f32()
    return (i32)f + (i32)p.second
}
