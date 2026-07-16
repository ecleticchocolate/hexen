//@ expect val 0
struct Flag { bool on }
impl Flag {
    fn __not() bool { return !self.on }
}
fn main() i32 {
    Flag f = { .on = true }
    if (!f) { return 1 }
    return 0
}
