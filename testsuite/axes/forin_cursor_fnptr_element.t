//@ expect val 9
// Axis cross: for-in cursor protocol where the element/payload type is
// itself a function pointer (fn(i32) i32), not a value or struct.
enum Option[T] { T Some  None }
struct FnList { (fn(i32)i32)[3] fns  u32 pos }
struct Cur { (fn(i32)i32)[3] fns  u32 pos }
impl FnList { fn begin() Cur { return {.fns = self.fns, .pos = 0} } }
impl Cur {
    fn next() Option[fn(i32) i32] {
        if self.pos >= 3 { return .None }
        fn(i32) i32 f = self.fns[self.pos]
        self.pos = self.pos + 1
        return .Some(f)
    }
}
fn double(i32 x) i32 { return x * 2 }
fn triple(i32 x) i32 { return x * 3 }
fn quad(i32 x) i32 { return x * 4 }
fn main() i32 {
    FnList l = {.fns = {double, triple, quad}, .pos = 0}
    i32 sum = 0
    for fn(i32) i32 f in l { sum = sum + f(1) }
    return sum
}
