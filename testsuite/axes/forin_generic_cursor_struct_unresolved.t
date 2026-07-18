//@ expect val 15
// Regression: for-in's cursor protocol resolves begin()/next() through the
// receiver's GENERIC BASE symbol (so `Ring[i32,5]`'s begin lives under
// `Ring_begin`, not a per-instantiation name). That lookup previously left
// the resolved return types (Cur[T,N], then Option[T]) in terms of the
// base's own bare type-param symbols instead of substituting the concrete
// instantiation's args -- so the loop element type came out as the literal
// symbol `T`, and any for-in over a GENERIC Cursor struct failed to compile
// with "cannot assign T to i32". Fixed via specialize_return() in
// parse_forin_statement (parser.c).
enum Option[T] { T Some  None }
struct Ring[T, u32 N] { T[N] items  u32 pos }
struct Cur[T, u32 N] { T[N] items  u32 pos }
impl Ring[T, u32 N] {
    fn begin() Cur[T, N] { return {.items = self.items, .pos = 0} }
}
impl Cur[T, u32 N] {
    fn next() Option[T] {
        if self.pos >= N { return .None }
        T v = self.items[self.pos]
        self.pos = self.pos + 1
        return .Some(v)
    }
}
fn main() i32 {
    Ring[i32, 5] r = {.items = {1,2,3,4,5}, .pos = 0}
    i32 sum = 0
    for i32 x in r { sum = sum + x }
    return sum
}
