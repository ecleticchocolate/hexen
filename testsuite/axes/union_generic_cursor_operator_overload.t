//@ expect val 30
// Union crossed with the cursor protocol (untested combination -- unions
// haven't touched for-in at all) and operator overload.
enum Option[T] { T Some  None }
union Num { i32 i  f32 f }
struct Box[T] { T[3] items  u32 pos }
struct Cur[T] { T[3] items  u32 pos }
impl Box[T] { fn begin() Cur[T] { return {.items = self.items, .pos = 0} } }
impl Cur[T] {
    fn next() Option[T] {
        if self.pos >= 3 { return .None }
        T v = self.items[self.pos]
        self.pos = self.pos + 1
        return .Some(v)
    }
}
impl Num {
    fn __add(Num other) Num { return {.i = self.i + other.i} }
}
fn main() i32 {
    Box[Num] b = {.items = { {.i=1}, {.i=2}, {.i=3} }, .pos = 0}
    Num acc = {.i = 0}
    for Num n in b {
        acc = acc + n
    }
    return acc.i * 5
}
