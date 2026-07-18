//@ expect val 20
enum Option[T] { T Some  None }
struct Iter { i32 max  i32 cur }
impl Iter { fn begin() Iter { return {.max = self.max, .cur = self.cur} } }
impl Iter {
    fn next() Option[i32] {
        if self.cur >= self.max { return .None }
        i32 v = self.cur
        self.cur = self.cur + 2
        return .Some(v)
    }
}
fn main() i32 {
    Iter iter = {.max = 10, .cur = 0}
    Iter* p = &iter
    i32 sum = 0
    for i32 x in p[0] {
        sum = sum + x
    }
    return sum
}
