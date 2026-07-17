//@ expect val 6
enum Option[T] { T Some  None }
struct P { i32 x  i32 y }
struct Gen { i32 i  i32 lim }
impl Gen { fn begin() Gen { return {.i = self.i, .lim = self.lim} } }
impl Gen { fn next() Option[P] { if self.i >= self.lim { return .None } i32 c = self.i  self.i = self.i + 1  return .Some{ {.x = c, .y = c} } } }
fn main() i32 { Gen g = {.i = 1, .lim = 3}  i32 s = 0  for P pt in g { s = s + pt.x + pt.y }  return s }
