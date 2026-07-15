//@ expect val 3
struct Counter { u32 val }
impl Counter {
    fn inc() { self.val = self.val + 1 }
    fn get() u32 { return self.val }
}
fn main() i32 {
    Counter c = {.val = 0}
    c.inc()
    c.inc()
    c.inc()
    return (i32)c.get()
}
