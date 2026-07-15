//@ expect val 12
struct Counter { u32 val }
impl Counter {
    fn inc() { self.val = self.val + 1 }
    fn get() u32 { return self.val }
}
fn main() i32 {
    Counter c = {.val = 10}
    Counter* p = &c
    p.inc()
    p.inc()
    return (i32)p.get()
}
