//@ expect val 6
struct Counter { u32 val }
pub impl Counter {
    fn inc() { self.val = self.val + 1 }
    fn get() u32 { return self.val }
}
fn main() i32 {
    Counter c = {.val = 5}
    c.inc()
    return (i32)c.get()
}
