//@ expect val 15
enum Option[T] { T Some  None }
struct Iter { i32 limit  i32 curr }
impl Iter { fn begin() Iter { return {.limit = self.limit, .curr = self.curr} } }
impl Iter {
    fn next() Option[i32] {
        if self.curr >= self.limit { return .None }
        i32 v = self.curr
        self.curr = self.curr + 1
        return .Some{v}
    }
}
fn make_iter(i32 limit) Iter { return {.limit = limit, .curr = 0} }
fn main() i32 {
    i32 sum = 0
    for i32 x in make_iter(6) {
        sum = sum + x
    }
    return sum
}
