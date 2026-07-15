//@ expect val 30
struct A {
   u32 x
}
struct B {
   super A base
   u32 y
}
impl B {
    fn sum() i32 { return self.x + self.y }
}
fn main() i32 {
    B b
    b.x = 10
    b.y = 20
    return b.sum()
}
