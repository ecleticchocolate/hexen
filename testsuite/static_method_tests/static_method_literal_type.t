//@ expect val 7
// `static fn` on a literal struct name: no self injected, called as
// Type.method(...) rather than instance.method(...). Exercises both a
// zero-arg and a multi-arg static constructor-style function, plus that
// an ordinary instance method on the same struct is untouched by the
// same call-rewrite path.
struct Point { i32 x  i32 y }
impl Point {
    static fn origin() Point { return {.x = 0, .y = 0} }
    static fn make(i32 x, i32 y) Point { return {.x = x, .y = y} }
    fn sum() i32 { return self.x + self.y }
}
fn main() i32 {
    Point p = Point.origin()
    Point q = Point.make(3, 4)
    return p.sum() + q.sum()
}
