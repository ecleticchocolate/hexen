//@ expect val 21
// Axis cross: unpack-pattern destructuring of a for-in cursor's payload,
// where the Cursor itself is generic (same shape as the specialize_return
// fix in forin_generic_cursor_struct_unresolved.t, layered with pattern
// destructuring instead of a plain typed loop var).
enum Option[T] { T Some  None }
struct Pair[T] { T a  T b }
struct Boxes[T] { Pair[T][2] items  u32 pos }
struct Cur[T] { Pair[T][2] items  u32 pos }
impl Boxes[T] { fn begin() Cur[T] { return {.items = self.items, .pos = 0} } }
impl Cur[T] {
    fn next() Option[Pair[T]] {
        if self.pos >= 2 { return .None }
        Pair[T] p = self.items[self.pos]
        self.pos = self.pos + 1
        return .Some(p)
    }
}
fn main() i32 {
    Boxes[i32] bx = {.items = { {.a=1,.b=2}, {.a=3,.b=15} }, .pos = 0}
    i32 sum = 0
    for unpack { .a = x, .b = y } in bx {
        sum = sum + x + y
    }
    return sum
}
