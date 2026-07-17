//@ expect val 48
enum Option[T] { T Some  None }
struct Iter { i32 max  i32 cur }
impl Iter { fn begin() Iter { return {.max = self.max, .cur = 0} } }
impl Iter {
    fn next() Option[i32] {
        if self.cur >= self.max { return .None }
        i32 v = self.cur
        self.cur = self.cur + 1
        return .Some{v}
    }
}
fn main() i32 {
    Iter iter = {.max = 4, .cur = 0}
    i32 sum = 0
    for i32 x in iter {
        for i32 y in iter {
            sum = sum + x + y
        }
    }
    return sum
}
