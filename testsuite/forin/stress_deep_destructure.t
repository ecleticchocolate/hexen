//@ expect val 42
enum Option[T] { T Some  None }
struct Inner { i32 a  i32 b }
struct Outer { Inner in1  Inner in2 }
struct List { Outer[2] arr  u32 ix }
impl List { fn begin() List { return {.arr = self.arr, .ix = self.ix} } }
impl List {
    fn next() Option[Outer] {
        if self.ix >= 2 { return .None }
        Outer o = self.arr[self.ix]
        self.ix = self.ix + 1
        return .Some{o}
    }
}
fn main() i32 {
    List l = {.arr = { {.in1={.a=1,.b=2}, .in2={.a=3,.b=4}}, {.in1={.a=5,.b=6}, .in2={.a=10,.b=11}} }, .ix = 0}
    i32 sum = 0
    for unpack { .in1 = { .a = a1, .b = b1 }, .in2 = { .a = a2, .b = b2 } } in l {
        sum = sum + a1 + b1 + a2 + b2
    }
    return sum
}
