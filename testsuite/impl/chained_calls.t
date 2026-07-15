//@ expect val 10
struct Builder { i32 val }
impl Builder {
    fn add(i32 n) { self.val = self.val + n }
    fn get() i32 { return self.val }
}
fn main() i32 {
    Builder b = {.val = 0}
    b.add(1)
    b.add(2)
    b.add(3)
    b.add(4)
    return b.get()
}
