//@ expect val 6
enum Maybe[T] { T Present  Absent }
struct Rng { i32 lim }
struct RC { i32 i  i32 lim }
impl Rng { fn begin() RC { return {.i = 0, .lim = self.lim} } }
impl RC { fn next() Maybe[i32] { if self.i >= self.lim { return .Absent } i32 cur = self.i  self.i = self.i + 1  return .Present(cur) } }
fn main() i32 {
    Rng r = {.lim = 4}
    i32 s = 0
    for i32 x in r { s = s + x }
    return s
}
