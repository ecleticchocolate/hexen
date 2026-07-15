//@ expect val 11
struct Box[T] { T val }
impl Box[T] {
    fn get() T { return self.val }
    fn get2() T { return self.get() }
    fn get3() T { return self.get2() }
}
fn main() i32 {
    Box[i32] b = {.val = 11}
    return b.get3()
}
