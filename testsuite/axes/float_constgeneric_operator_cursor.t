//@ expect stdout
//@ | sum=16.500000
extern fn printf(u8* fmt, ...) i32
enum Option[T] { T Some  None }
struct Vec[u32 N] { f64[N] v }
struct Cur[u32 N] { f64[N] v  u32 pos }
impl Vec[u32 N] {
    fn begin() Cur[N] { return {.v = self.v, .pos = 0} }
    fn __add(Vec[N] other) Vec[N] {
        Vec[N] r
        u32 i = 0
        while i < N { r.v[i] = self.v[i] + other.v[i]  i = i + 1 }
        return r
    }
}
impl Cur[u32 N] {
    fn next() Option[f64] {
        if self.pos >= N { return .None }
        f64 x = self.v[self.pos]
        self.pos = self.pos + 1
        return .Some(x)
    }
}
fn main() i32 {
    Vec[3] a = {.v = {1.5, 2.5, 3.0}}
    Vec[3] b = {.v = {0.5, 4.0, 5.0}}
    Vec[3] c = a + b
    f64 sum = 0.0
    for f64 x in c { sum = sum + x }
    printf("sum=%f\n", sum)
    return 0
}
